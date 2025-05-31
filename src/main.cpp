
#include <fmt/core.h>

int main() {
    fmt::print("Hello, world!\n");
    return 0;
=======
#include <iostream>
#include <string>
#include <curl/curl.h> // For libcurl functionalities

// Callback function to write data received from libcurl into a std::string
// libcurl calls this function when data is received from the server.
// - contents: pointer to the received data
// - size: size of each data member (usually 1)
// - nmemb: number of data members
// - s: pointer to the std::string object where data will be appended
// Returns the total number of bytes handled. If it differs from size*nmemb,
// libcurl will consider it an error.
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength); // Append the new data to the string
    } catch(std::bad_alloc &e) {
        // Handle memory allocation problem if string append fails
        std::cerr << "Error: Memory allocation failed in WriteCallback." << std::endl;
        return 0; // Signal an error to libcurl
    }
    return newLength; // Signal success to libcurl
}

// Helper function to URL-encode a string using libcurl
// URL encoding ensures that data sent in URLs or POST bodies is correctly interpreted,
// especially special characters.
// - curl: A CURL easy handle (used by curl_easy_escape)
// - value: The string to be URL-encoded
// Returns the URL-encoded string. Returns an empty string on failure.
std::string url_encode(CURL *curl, const std::string &value) {
    // curl_easy_escape URL-encodes the given string.
    // The returned string must be freed with curl_free.
    char *output = curl_easy_escape(curl, value.c_str(), value.length());
    if (output) {
        std::string result(output);
        curl_free(output); // Free the allocated memory
        return result;
    }
    return ""; // Return empty string if encoding fails
}

// Sends an SMS using the Twilio API.
// - account_sid: Your Twilio Account SID.
// - auth_token: Your Twilio Auth Token.
// - to_number: The recipient's phone number (E.164 format, e.g., +1234567890).
// - from_number: Your Twilio phone number (E.164 format).
// - message_body: The text of the SMS message.
// - api_response_str: A reference to a string to store the full API response from Twilio.
// Returns true if Twilio indicates success (HTTP 201), false otherwise.
bool send_sms(const std::string &account_sid,
              const std::string &auth_token,
              const std::string &to_number,
              const std::string &from_number,
              const std::string &message_body,
              std::string &api_response_str) {
    CURL *curl;       // libcurl easy handle
    CURLcode res;     // Result of libcurl operations
    bool success = false; // Assume failure initially

    api_response_str.clear(); // Clear any previous API response

    // Initialize libcurl globally. Should be called once per program.
    curl_global_init(CURL_GLOBAL_ALL);
    // Get a libcurl easy handle. This handle is used for all subsequent transfers.
    curl = curl_easy_init();

    if (curl) {
        // Construct the Twilio API URL for sending messages.
        // {ACCOUNT_SID} is replaced by your actual Account SID.
        std::string url = "https://api.twilio.com/2010-04-01/Accounts/" + account_sid + "/Messages.json";

        // Construct the POST data for the Twilio API.
        // Parameters must be URL-encoded.
        std::string post_data = "To=" + url_encode(curl, to_number) +
                                "&From=" + url_encode(curl, from_number) +
                                "&Body=" + url_encode(curl, message_body);

        // Set libcurl options:
        // CURLOPT_URL: The URL to send the request to.
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // CURLOPT_POSTFIELDS: The data to send in the HTTP POST request.
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        // CURLOPT_USERNAME: The username for HTTP Basic Authentication (Twilio Account SID).
        curl_easy_setopt(curl, CURLOPT_USERNAME, account_sid.c_str());
        // CURLOPT_PASSWORD: The password for HTTP Basic Authentication (Twilio Auth Token).
        curl_easy_setopt(curl, CURLOPT_PASSWORD, auth_token.c_str());
        // CURLOPT_USERAGENT: A user agent string for the request.
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "cpp-sms-app/1.0");
        // CURLOPT_VERBOSE: Set to 1L for detailed debugging output from libcurl (useful for troubleshooting).
        // Commented out for cleaner production output.
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // Set up the callback function to handle the response data from Twilio.
        // CURLOPT_WRITEFUNCTION: Points to the function that will handle incoming data.
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        // CURLOPT_WRITEDATA: Points to the std::string where WriteCallback will store the data.
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &api_response_str);

        // Perform the HTTP POST request. This is a blocking call.
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            // If curl_easy_perform fails, print the libcurl error.
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            // If the request was successful, get the HTTP response code.
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            std::cout << "HTTP response code: " << http_code << std::endl;
            std::cout << "API Response: " << api_response_str << std::endl; // Print the full API response

            // Twilio uses HTTP 201 Created to indicate successful message queuing.
            if (http_code == 201) {
                std::cout << "SMS successfully queued by Twilio." << std::endl;
                success = true; // Mark as successful
            } else {
                // If not 201, it's an error or unexpected response.
                std::cerr << "SMS sending failed. Twilio responded with HTTP " << http_code << "." << std::endl;
            }
        }

        // Clean up the libcurl easy handle.
        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize libcurl easy handle." << std::endl;
    }

    // Clean up libcurl global state. Should be called once per program.
    curl_global_cleanup();
    return success; // Return the success status
}

// Validates a phone number string.
// Basic validation: checks if it's not empty and starts with '+'.
// For more robust validation, a regex library or more specific checks would be needed.
// - number: The phone number string to validate.
// Returns true if the number seems valid, false otherwise.
bool is_valid_phone_number(const std::string& number) {
    if (number.empty()) return false; // Cannot be empty
    // A common convention for E.164 format is starting with '+'.
    // Length check is also basic; real numbers have varying lengths.
    return number[0] == '+' && number.length() > 5; // Example: +12345 (at least 6 chars)
}

// Main function: Entry point of the application.
// Prompts the user for Twilio credentials and SMS details, then calls send_sms.
int main() {
    std::string account_sid;
    std::string auth_token;
    std::string to_number;
    std::string from_number;
    std::string message_body;
    std::string api_response; // To store response from send_sms

    std::cout << "--- C++ SMS Sender using Twilio ---" << std::endl;

    // Input loop for Account SID with validation
    while(true) {
        std::cout << "Enter your Twilio Account SID: ";
        std::getline(std::cin, account_sid); // Read full line to handle spaces if any
        // Basic validation: non-empty and typically starts with "AC".
        if (!account_sid.empty() && (account_sid.rfind("AC", 0) == 0) && account_sid.length() > 30) {
            break; // Valid input, exit loop
        }
        std::cerr << "Invalid Account SID format. It should start with 'AC', be non-empty, and typically >30 chars." << std::endl;
    }

    // Input loop for Auth Token with validation
    while(true) {
        std::cout << "Enter your Twilio Auth Token: ";
        std::getline(std::cin, auth_token);
        if (!auth_token.empty()) { // Basic validation: non-empty
            break; // Valid input, exit loop
        }
        std::cerr << "Auth Token cannot be empty." << std::endl;
    }

    // Input loop for recipient's phone number with validation
    while(true) {
        std::cout << "Enter the recipient's phone number (e.g., +1234567890): ";
        std::getline(std::cin, to_number);
        if (is_valid_phone_number(to_number)) { // Use helper for validation
            break; // Valid input, exit loop
        }
        std::cerr << "Invalid recipient phone number format. It must start with '+' and be non-empty (e.g., +1234567890)." << std::endl;
    }

    // Input loop for Twilio phone number with validation
    while(true) {
        std::cout << "Enter your Twilio phone number (e.g., +10987654321): ";
        std::getline(std::cin, from_number);
        if (is_valid_phone_number(from_number)) { // Use helper for validation
            break; // Valid input, exit loop
        }
        std::cerr << "Invalid Twilio phone number format. It must start with '+' and be non-empty (e.g., +10987654321)." << std::endl;
    }

    // Get the message body from the user
    std::cout << "Enter the message body: ";
    std::getline(std::cin, message_body);
    if (message_body.empty()) {
        // Optionally, allow empty messages or add validation if they should not be empty.
        std::cout << "Warning: Message body is empty." << std::endl;
    }

    // Attempt to send the SMS
    std::cout << "\nSending SMS..." << std::endl;
    if (send_sms(account_sid, auth_token, to_number, from_number, message_body, api_response)) {
        // If send_sms returns true, it means Twilio responded with HTTP 201.
        std::cout << "Message sending process completed successfully based on API response." << std::endl;
    } else {
        // If send_sms returns false, an error occurred (either libcurl or non-201 HTTP code).
        // Detailed error messages (including API response) would have been printed by send_sms.
        std::cerr << "Message sending process failed or API indicated an error." << std::endl;
    }

   return 0; // Indicate successful execution of the main function

}
