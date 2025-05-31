#include <iostream>
#include <string>
#include <cstring> // For strcpy, strlen, strcmp
#include <vector>
#include <microhttpd.h>
#include <curl/curl.h> // For libcurl functionalities

// --- Globals for storing parsed POST data (for webhook receive mode) ---
static std::string g_webhook_from_number;
static std::string g_webhook_message_body;
static bool g_was_post_request; // Flag to indicate if current request was POST

// --- Libcurl SMS sending functions (for send mode) ---

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch(std::bad_alloc &e) {
        std::cerr << "Error: Memory allocation failed in WriteCallback." << std::endl;
        return 0;
    }
    return newLength;
}

static std::string url_encode_for_curl(CURL *curl, const std::string &value) {
    char *output = curl_easy_escape(curl, value.c_str(), value.length());
    if (output) {
        std::string result(output);
        curl_free(output);
        return result;
    }
    return "";
}

static bool send_sms(const std::string &account_sid,
                     const std::string &auth_token,
                     const std::string &to_number,
                     const std::string &from_number,
                     const std::string &message_body,
                     std::string &api_response_str) {
    CURL *curl;
    CURLcode res;
    bool success = false;

    api_response_str.clear();
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        std::string url = "https://api.twilio.com/2010-04-01/Accounts/" + account_sid + "/Messages.json";
        std::string post_data = "To=" + url_encode_for_curl(curl, to_number) +
                                "&From=" + url_encode_for_curl(curl, from_number) +
                                "&Body=" + url_encode_for_curl(curl, message_body);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, account_sid.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, auth_token.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "cpp-sms-app/1.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &api_response_str);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            std::cout << "HTTP response code: " << http_code << std::endl;
            std::cout << "API Response: " << api_response_str << std::endl;
            if (http_code == 201) {
                std::cout << "SMS successfully queued by Twilio." << std::endl;
                success = true;
            } else {
                std::cerr << "SMS sending failed. Twilio responded with HTTP " << http_code << "." << std::endl;
            }
        }
        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize libcurl." << std::endl;
    }
    curl_global_cleanup();
    return success;
}

static bool is_valid_phone_number_for_sending(const std::string& number) {
    if (number.empty()) return false;
    return number[0] == '+' && number.length() > 5;
}

// --- libmicrohttpd Webhook functions (for receive mode) ---

static enum MHD_Result post_iterator(void *cls,
                                     enum MHD_ValueKind kind,
                                     const char *key,
                                     const char *filename,
                                     const char *content_type,
                                     const char *transfer_encoding,
                                     const char *data,
                                     uint64_t off,
                                     size_t size) {
    (void)cls;
    (void)kind;
    (void)filename;
    (void)content_type;
    (void)transfer_encoding;
    (void)off; // Assuming data for a single key comes in one chunk or is small enough

    // Rudimentary check against excessively large individual POST values if needed
    // For example, if (size > SOME_MAX_FIELD_SIZE) { return MHD_NO; }

    if (size > 0 && key != NULL) { // Ensure key is not null before strcmp
        if (strcmp(key, "From") == 0) {
            if (g_webhook_from_number.length() + size > 100) { /* Some reasonable limit */ return MHD_NO; } // Prevent overflow
            g_webhook_from_number.append(data, size);
        } else if (strcmp(key, "Body") == 0) {
            if (g_webhook_message_body.length() + size > 2048) { /* Some reasonable limit */ return MHD_NO; } // Prevent overflow
            g_webhook_message_body.append(data, size);
        }
    }
    return MHD_YES;
}

static enum MHD_Result send_error_response(struct MHD_Connection *connection, const char *error_message, unsigned int status_code) {
    struct MHD_Response *response;
    enum MHD_Result ret;

    response = MHD_create_response_from_buffer(strlen(error_message), (void *)error_message, MHD_RESPMEM_PERSISTENT);
    if (!response) {
        std::cerr << "Fatal: Failed to create error response buffer." << std::endl;
        return MHD_NO; // Cannot even send an error
    }
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain");
    ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

static enum MHD_Result answer_to_connection(void *cls,
                                            struct MHD_Connection *connection,
                                            const char *url,
                                            const char *method,
                                            const char *version,
                                            const char *upload_data,
                                            size_t *upload_data_size,
                                            void **con_cls) {
    (void)cls;
    (void)version;

    g_webhook_from_number.clear();
    g_webhook_message_body.clear();
    g_was_post_request = false;

    std::cout << "\n--- New Request ---" << std::endl;
    std::cout << "URL: " << url << std::endl;
    std::cout << "Method: " << method << std::endl;

    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
        g_was_post_request = true;
        MHD_PostProcessor *post_processor = (MHD_PostProcessor*)*con_cls;

        if (*con_cls == NULL) { // First call for this connection to process POST data
            post_processor = MHD_create_post_processor(connection, 1024, &post_iterator, NULL); // 1KB buffer for POST data
            if (post_processor == NULL) {
                std::cerr << "Error: Failed to create MHD_PostProcessor." << std::endl;
                return send_error_response(connection, "Internal Server Error: Could not create post processor.", MHD_HTTP_INTERNAL_SERVER_ERROR);
            }
            *con_cls = (void *)post_processor;
            // Since data might already be in upload_data for the first call, proceed to process it.
            // No need to return MHD_YES immediately unless *upload_data_size == 0 here.
        }
        // else: post_processor already exists from *con_cls

        if (*upload_data_size != 0) { // If there's data in the current chunk
            if (MHD_post_process(post_processor, upload_data, *upload_data_size) != MHD_YES) {
                 std::cerr << "Error: MHD_post_process failed." << std::endl;
                 // Post processor will be cleaned up by request_completed_callback
                 return send_error_response(connection, "Internal Server Error: Failed to process POST data.", MHD_HTTP_INTERNAL_SERVER_ERROR);
            }
            *upload_data_size = 0; // Mark data as processed for this call
            return MHD_YES;        // Tell libmicrohttpd to continue expecting more data if any
        } else { // No more data in this call, or POST request with empty body initially.
                 // This block signifies the end of data receiving for the request.
            if (g_was_post_request) { // Only log specific fields if it was a POST
                 if (!g_webhook_from_number.empty() || !g_webhook_message_body.empty()) {
                    std::cout << "-------------------------" << std::endl;
                    std::cout << "Incoming Message Data:" << std::endl;
                    std::cout << "  From: " << g_webhook_from_number << std::endl;
                    std::cout << "  Body: " << g_webhook_message_body << std::endl;
                    std::cout << "-------------------------" << std::endl;
                } else {
                    std::cout << "  (POST request body processed. 'From' or 'Body' fields not found or were empty.)" << std::endl;
                }
            }
        }
    } else { // Not a POST request
        std::cout << "  (Not a POST request, no specific data to parse this way)" << std::endl;
    }

    const char *page_content = "Webhook received successfully."; // Generic success message
    struct MHD_Response *response;
    enum MHD_Result ret;

    response = MHD_create_response_from_buffer(strlen(page_content), (void *)page_content, MHD_RESPMEM_PERSISTENT);
    if (!response) {
        std::cerr << "Fatal: Failed to create success response buffer." << std::endl;
        // No client response possible if this fails
        return MHD_NO;
    }

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    std::cout << "--- Request End ---" << std::endl;
    return ret;
}

static void request_completed_callback(void *cls, struct MHD_Connection *connection,
                                       void **con_cls, enum MHD_RequestTerminationCode toe) {
    (void)cls; (void)connection; (void)toe;
    MHD_PostProcessor *post_processor = (MHD_PostProcessor*)*con_cls;
    if (post_processor) {
        MHD_destroy_post_processor(post_processor);
        *con_cls = NULL;
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cout << "Starting in Receive Mode (Webhook Server)..." << std::endl;
        struct MHD_Daemon *daemon;
        unsigned int flags = MHD_USE_SELECT_INTERNALLY;

        daemon = MHD_start_daemon(flags, 8080, NULL, NULL,
                                  &answer_to_connection, NULL,
                                  MHD_OPTION_NOTIFY_COMPLETED, &request_completed_callback, NULL,
                                  MHD_OPTION_END);

        if (NULL == daemon) {
            std::cerr << "Error: Failed to start libmicrohttpd server." << std::endl;
            std::cerr << "Possible reasons: Port 8080 already in use, insufficient permissions, or system resource limits." << std::endl;
            return 1;
        }

        std::cout << "Server started on port 8080..." << std::endl;
        std::cout << "Press Enter to stop the server." << std::endl;
        getchar();
        std::cout << "Stopping server..." << std::endl;
        MHD_stop_daemon(daemon);

    } else if (argc == 2 && strcmp(argv[1], "--send") == 0) {
        std::cout << "Starting in Send Mode..." << std::endl;
        std::string account_sid, auth_token, to_number, from_number, message_body, api_response;
        std::cout << "--- C++ SMS Sender using Twilio ---" << std::endl;
        while(true) {
            std::cout << "Enter your Twilio Account SID: ";
            std::getline(std::cin, account_sid);
            if (!account_sid.empty() && (account_sid.rfind("AC", 0) == 0) && account_sid.length() > 30) break;
            std::cerr << "Invalid Account SID format. It should start with 'AC', be non-empty, and >30 chars." << std::endl;
        }
        while(true) {
            std::cout << "Enter your Twilio Auth Token: ";
            std::getline(std::cin, auth_token);
            if (!auth_token.empty()) break;
            std::cerr << "Auth Token cannot be empty." << std::endl;
        }
        while(true) {
            std::cout << "Enter the recipient's phone number (e.g., +1234567890): ";
            std::getline(std::cin, to_number);
            if (is_valid_phone_number_for_sending(to_number)) break;
            std::cerr << "Invalid recipient phone number format. It must start with '+' and be non-empty." << std::endl;
        }
        while(true) {
            std::cout << "Enter your Twilio phone number (e.g., +10987654321): ";
            std::getline(std::cin, from_number);
            if (is_valid_phone_number_for_sending(from_number)) break;
            std::cerr << "Invalid Twilio phone number format. It must start with '+' and be non-empty." << std::endl;
        }
        std::cout << "Enter the message body: ";
        std::getline(std::cin, message_body);
        if (message_body.empty()) { std::cout << "Warning: Message body is empty." << std::endl; }
        std::cout << "\nSending SMS..." << std::endl;
        if (send_sms(account_sid, auth_token, to_number, from_number, message_body, api_response)) {
            std::cout << "Message sending process completed successfully based on API response." << std::endl;
        } else {
            std::cerr << "Message sending process failed or API indicated an error." << std::endl;
        }
    } else {
        std::cerr << "Usage: " << argv[0] << " [--send]" << std::endl;
        std::cerr << "  (no arguments)    : Start in receive mode (webhook server)." << std::endl;
        std::cerr << "  --send            : Start in send mode (CLI for sending SMS)." << std::endl;
        return 1;
    }
    return 0;
}
