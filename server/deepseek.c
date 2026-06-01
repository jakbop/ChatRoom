/*
 * deepseek.c - DeepSeek API 调用模块实现
 *
 * 实现原理：
 *   1. 使用 libcurl 发送 HTTPS POST 请求到 DeepSeek API
 *   2. 请求体为 JSON 格式，兼容 OpenAI Chat Completions API
 *   3. 响应体为 JSON 格式，手动解析提取 AI 回复内容
 *   4. 无需第三方 JSON 库，使用简易字符串解析
 *
 * DeepSeek API 文档：https://platform.deepseek.com/api-docs
 *
 * 编译依赖：
 *   apt-get install libcurl4-openssl-dev
 *   gcc -c deepseek.c -o deepseek.o -lcurl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include "deepseek.h"

/* ========================= 宏定义 ========================= */
#define API_URL "https://api.deepseek.com/chat/completions"
#define MAX_RESPONSE_BUF 8192
#define MAX_REQUEST_BUF  8192

/* ========================= 响应缓冲区结构体 ========================= */
typedef struct {
    char *data;       /* 指向动态分配的缓冲区 */
    int   size;       /* 当前已写入的数据长度 */
    int   capacity;   /* 缓冲区总容量 */
} response_buf_t;

/* ==========================================================================
 * write_callback() - libcurl 写入回调函数
 *
 * 当 libcurl 接收到 HTTP 响应体数据时，会调用此函数。
 * 我们将数据追加到 response_buf_t 缓冲区中。
 *
 * 参数：
 *   ptr     - 指向接收到的数据
 *   size    - 每个数据块的大小（始终为1）
 *   nmemb   - 数据块的数量
 *   userp   - 用户自定义指针，指向 response_buf_t
 *
 * 返回值：实际处理的字节数（必须等于 size * nmemb，否则 libcurl 会报错）
 * ========================================================================== */
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    response_buf_t *buf = (response_buf_t *)userp;
    int total = size * nmemb;

    /* 检查是否需要扩容 */
    if (buf->size + total + 1 > buf->capacity)
    {
        int new_cap = buf->capacity * 2;
        if (new_cap < buf->size + total + 1)
            new_cap = buf->size + total + 1;

        char *new_data = (char *)realloc(buf->data, new_cap);
        if (!new_data)
            return 0;

        buf->data = new_data;
        buf->capacity = new_cap;
    }

    /* 追加数据到缓冲区 */
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';

    return total;
}

/* ==========================================================================
 * json_extract_string() - 从 JSON 字符串中提取指定键的值
 *
 * 简易 JSON 解析器，不依赖第三方库。
 * 仅适用于 DeepSeek API 返回的简单 JSON 结构。
 *
 * 查找逻辑：
 *   在 json_str 中搜索 "key":，然后提取冒号后的字符串值
 *   支持提取带引号的字符串值和未带引号的值
 *
 * 参数：
 *   json_str - JSON 字符串
 *   key      - 要查找的键名
 *   output   - 输出缓冲区
 *   out_len  - 输出缓冲区大小
 *
 * 返回值：0 成功，-1 未找到
 * ========================================================================== */
static int json_extract_string(const char *json_str, const char *key,
                               char *output, int out_len)
{
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char *pos = strstr(json_str, search_key);
    if (!pos)
        return -1;

    pos += strlen(search_key);

    while (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\n' || *pos == '\r')
        pos++;

    if (*pos == '"')
    {
        pos++;
        int i = 0;
        while (*pos && *pos != '"' && i < out_len - 1)
        {
            if (*pos == '\\' && *(pos + 1))
            {
                char next = *(pos + 1);
                if (next == 'n')       { output[i++] = '\n'; pos += 2; }
                else if (next == 't')  { output[i++] = '\t'; pos += 2; }
                else if (next == '"')  { output[i++] = '"';  pos += 2; }
                else if (next == '\\') { output[i++] = '\\'; pos += 2; }
                else                   { output[i++] = *pos++; }
            }
            else
            {
                output[i++] = *pos++;
            }
        }
        output[i] = '\0';
        return 0;
    }

    return -1;
}

/* ==========================================================================
 * json_extract_content() - 从 DeepSeek API 响应中提取 AI 回复内容
 *
 * DeepSeek API 响应格式：
 * {
 *   "choices": [{
 *     "message": {
 *       "content": "AI的回复内容"
 *     }
 *   }]
 * }
 *
 * 解析策略：
 *   1. 找到 "content" 键
 *   2. 提取其后的字符串值
 *   3. 处理转义字符（\n, \t, \", \\）
 *
 * 参数：
 *   json_str - API 返回的完整 JSON 响应
 *   output   - 输出缓冲区
 *   out_len  - 输出缓冲区大小
 *
 * 返回值：0 成功，-1 解析失败
 * ========================================================================== */
static int json_extract_content(const char *json_str, char *output, int out_len)
{
    const char *choices_pos = strstr(json_str, "\"choices\"");
    if (!choices_pos)
        return -1;

    const char *content_pos = strstr(choices_pos, "\"content\"");
    if (!content_pos)
        return -1;

    return json_extract_string(content_pos, "content", output, out_len);
}

/* ==========================================================================
 * deepseek_ask_with_system() - 带自定义 system prompt 的 API 调用
 *
 * 完整流程：
 *   1. 初始化 libcurl
 *   2. 构造 JSON 请求体
 *   3. 设置 HTTP 头（Authorization、Content-Type）
 *   4. 发送 HTTPS POST 请求
 *   5. 接收并解析 JSON 响应
 *   6. 提取 AI 回复内容
 *
 * 参数：见 deepseek.h 中的声明
 * ========================================================================== */
int deepseek_ask_with_system(const char *api_key, const char *system_prompt,
                             const char *question,
                             char *response, int resp_len)
{
    CURL *curl = NULL;
    CURLcode res;
    struct curl_slist *headers = NULL;
    response_buf_t resp_buf = {NULL, 0, MAX_RESPONSE_BUF};
    int ret = -1;

    /* 初始化响应缓冲区 */
    resp_buf.data = (char *)malloc(MAX_RESPONSE_BUF);
    if (!resp_buf.data)
        return -1;
    resp_buf.data[0] = '\0';

    /* 初始化 libcurl */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl)
    {
        free(resp_buf.data);
        curl_global_cleanup();
        return -1;
    }

    /* 构造 JSON 请求体
     * 格式兼容 OpenAI Chat Completions API：
     * {
     *   "model": "deepseek-v4-flash",
     *   "messages": [
     *     {"role": "system", "content": "系统提示词"},
     *     {"role": "user", "content": "用户问题"}
     *   ],
     *   "max_tokens": 512,
     *   "temperature": 0.7
     * }
     */
    char json_body[MAX_REQUEST_BUF];
    snprintf(json_body, sizeof(json_body),
        "{"
        "\"model\":\"deepseek-v4-flash\","
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\"%s\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}"
        "],"
        "\"max_tokens\":512,"
        "\"temperature\":0.7"
        "}",
        system_prompt, question
    );

    /* 设置 HTTP 请求头 */
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    /* 配置 curl 选项 */
    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    /* 发送请求 */
    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "DeepSeek API 请求失败: %s\n", curl_easy_strerror(res));
        goto cleanup;
    }

    /* 检查 HTTP 状态码 */
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200)
    {
        fprintf(stderr, "DeepSeek API 返回错误 HTTP %ld: %s\n", http_code, resp_buf.data);

        char error_msg[256] = {0};
        if (json_extract_string(resp_buf.data, "message", error_msg, sizeof(error_msg)) == 0)
        {
            snprintf(response, resp_len, "AI 服务错误: %s", error_msg);
        }
        else
        {
            snprintf(response, resp_len, "AI 服务错误 (HTTP %ld)", http_code);
        }
        ret = -1;
        goto cleanup;
    }

    /* 解析 JSON 响应，提取 AI 回复内容 */
    if (json_extract_content(resp_buf.data, response, resp_len) != 0)
    {
        fprintf(stderr, "DeepSeek API 响应解析失败: %s\n", resp_buf.data);
        snprintf(response, resp_len, "AI 回复解析失败");
        ret = -1;
        goto cleanup;
    }

    ret = 0;

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(resp_buf.data);
    curl_global_cleanup();

    return ret;
}

/* ==========================================================================
 * deepseek_ask() - 调用 DeepSeek API（使用默认 system prompt）
 *
 * 默认 system prompt：
 *   "你是一个友好的聊天室AI助手，回答简洁有趣，用中文回答，每次回答不超过200字"
 *
 * 参数：见 deepseek.h 中的声明
 * ========================================================================== */
int deepseek_ask(const char *api_key, const char *question,
                 char *response, int resp_len)
{
    /* 动态生成 system prompt，包含当前日期 */
    char system_prompt[512];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%Y年%m月%d日", t);

    /* 获取星期几 */
    const char *weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

    snprintf(system_prompt, sizeof(system_prompt),
        "你是一个友好的聊天室AI助手，回答简洁有趣，用中文回答，每次回答不超过200字。"
        "今天是%s %s，请基于这个日期回答时间相关问题。",
        date_str, weekdays[t->tm_wday]);
    return deepseek_ask_with_system(
        api_key,
        system_prompt,
        question,
        response,
        resp_len
    );
}
