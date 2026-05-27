/*
 * deepseek.h - DeepSeek API 调用模块头文件
 *
 * 功能：
 *   封装 DeepSeek 大模型 API 的 HTTP 调用，支持：
 *   - 发送用户问题到 DeepSeek API
 *   - 解析 JSON 响应，提取 AI 回复内容
 *   - 支持自定义 system prompt
 *
 * 依赖：
 *   - libcurl (HTTPS 请求)
 *   - 自行实现的简易 JSON 解析（无需 cJSON 库）
 *
 * 使用方式：
 *   1. 在服务端代码中 #include "deepseek.h"
 *   2. 调用 deepseek_ask() 发送问题，获取回复
 *   3. 编译时需链接 -lcurl
 */

#ifndef DEEPSEEK_H
#define DEEPSEEK_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * deepseek_ask() - 调用 DeepSeek API 获取 AI 回复
 *
 * 参数：
 *   api_key  - DeepSeek API Key（从 platform.deepseek.com 获取）
 *   question - 用户提问内容（UTF-8 编码）
 *   response - 输出缓冲区，用于存储 AI 回复
 *   resp_len - response 缓冲区大小（字节）
 *
 * 返回值：
 *   0  - 成功，response 中存储了 AI 回复
 *   -1 - 失败（网络错误、API 错误、JSON 解析错误等）
 *
 * 注意：
 *   - 本函数是同步阻塞调用，耗时 1-5 秒
 *   - 建议在独立线程中调用，避免阻塞通信线程
 *   - API 调用会产生费用，建议做限流控制
 */
int deepseek_ask(const char *api_key, const char *question,
                 char *response, int resp_len);

/*
 * deepseek_ask_with_system() - 带自定义 system prompt 的 API 调用
 *
 * 参数：
 *   api_key        - DeepSeek API Key
 *   system_prompt  - 系统提示词（定义 AI 的角色和行为）
 *   question       - 用户提问内容
 *   response       - 输出缓冲区
 *   resp_len       - 缓冲区大小
 *
 * 返回值：同 deepseek_ask()
 */
int deepseek_ask_with_system(const char *api_key, const char *system_prompt,
                             const char *question,
                             char *response, int resp_len);

#ifdef __cplusplus
}
#endif

#endif
