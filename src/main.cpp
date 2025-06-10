
#include <fmt/core.h>

int main() {
    fmt::print("Hello, world!\n");
    return 0;
=======
#include <iostream>
#include <string>
#include <vector>
#include <fstream> // For file I/O (ifstream, ofstream)
#include <sstream> // For string stream operations (istringstream)
#include <cstdio>      // For std::remove
#include <curl/curl.h> // For libcurl functionalities
#include <algorithm> // Required for std::find_if_not and transform for trim, and for std::all_of
#include <cctype>    // Required for std::isdigit, std::isspace
#include <cstdlib>   // Required for exit, EXIT_FAILURE
// #include <string> // Already included via iostream or other headers indirectly but good for explicitness if it were standalone.

// --- Mock SMS Behavior Control Enum ---
enum MockSmsBehavior {
    REAL,
    MOCK_SUCCESS,
    MOCK_AUTH_FAIL,
    MOCK_INVALID_TO_FAIL,
    MOCK_URL_ENCODE_FAIL,
    MOCK_PERFORM_FAIL
};

// --- TestContext Struct ---
struct TestContext {
    bool test_mode = false;
    std::string scenario_filepath;
    std::ifstream scenario_stream;
    std::istream* input_stream = &std::cin; // Default to std::cin

    std::ofstream redirect_out_file;
    std::ofstream redirect_err_file;
    std::streambuf* orig_cout_buf = nullptr;
    std::streambuf* orig_cerr_buf = nullptr;

    MockSmsBehavior mock_sms_behavior = REAL;
    long mock_response_code = 201; // Default for MOCK_SUCCESS
    std::string mock_api_response_str = "{\"sid\": \"SMmockedsuccessfulsid\", \"status\": \"queued\", \"error_code\": null, \"error_message\": null}"; // Default success JSON
};

TestContext g_test_ctx; // Global instance of TestContext

// Global constant for the configuration filename
const std::string CONFIG_FILENAME = "config.txt";

// Structure to hold configuration data
struct ConfigData {
    std::string account_sid;
    std::string auth_token;
    std::string from_number;
    bool loaded_successfully = false; // Flag to indicate if loading was successful
};

// Loads configuration from a file.
// - filename: The name of the configuration file to load.
// Returns a ConfigData struct. If loading fails or file not found,
// loaded_successfully will be false.
ConfigData load_config(const std::string& filename) {
    ConfigData config;
    std::ifstream infile(filename);

    if (!infile.is_open()) {
        std::cout << "\nINFO: Configuration file (" << filename << ") not found or cannot be opened. Credentials need to be entered manually." << std::endl;
        config.loaded_successfully = false;
        return config;
    }

    std::string line;
    // int fields_found = 0; // Replaced by checking presence of SID, Token, Number directly
    // Using separate boolean flags to ensure each essential key is found at least once.
    bool sid_found = false, token_found = false, number_found = false;

    while (std::getline(infile, line)) {
        line = trim_whitespace(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue; // Skip empty lines and comments
        }

        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            key = trim_whitespace(key);
            // Convert key to uppercase for case-insensitive comparison
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
            value = trim_whitespace(value);

            if (key == "ACCOUNT_SID") {
                config.account_sid = value;
                if (!value.empty()) sid_found = true; // Mark as found only if value is not empty
            } else if (key == "AUTH_TOKEN") {
                config.auth_token = value;
                if (!value.empty()) token_found = true; // Mark as found only if value is not empty
            } else if (key == "FROM_NUMBER") {
                config.from_number = value;
                if (!value.empty()) number_found = true; // Mark as found only if value is not empty
            }
        }
    }
    infile.close();

    // Check if all essential fields were found and have non-empty values
    if (sid_found && token_found && number_found &&
        !config.account_sid.empty() && !config.auth_token.empty() && !config.from_number.empty()) {
        config.loaded_successfully = true;
        // Suppress successful load message for cleaner test output, user sees it via displayed values or lack thereof.
        // std::cout << "\nINFO: Configuration loaded successfully from " << filename << "." << std::endl;
    } else {
        config.loaded_successfully = false;
        // Only print error if the file was actually problematic, not just not found (handled at the start)
        if (infile.eof() && !(sid_found && token_found && number_found &&
                             !config.account_sid.empty() && !config.auth_token.empty() && !config.from_number.empty())) {
             if (filename != CONFIG_FILENAME) { // Avoid printing this for the actual config file during normal run, only for tests
                std::cout << "\nINFO: Test configuration file (" << filename << ") processed. Issues found (e.g. incomplete, malformed, empty values)." << std::endl;
             } else if (config.loaded_successfully == false && (sid_found || token_found || number_found)) { // If some fields were found but not all, it's an error for actual config
                std::cout << "\nERROR: Configuration file (" << filename << ") is incomplete or values are missing. Credentials need to be entered manually." << std::endl;
             }
        }
        // Reset all fields if loading was not fully successful
        config.account_sid.clear();
        config.auth_token.clear();
        config.from_number.clear();
    }
    return config;
}

// Helper function to mask the auth token for display
// Shows first 3, last 3 chars, and asterisks in between.
std::string mask_auth_token(const std::string& token) {
    if (token.length() < 8) { // Arbitrary length, if too short, just mask all
        return std::string(token.length(), '*');
    }
    return token.substr(0, 3) + "****" + token.substr(token.length() - 3);
}

// Function to remove leading and trailing whitespace
// std::isspace handles space, tab, newline, vertical tab, form feed, carriage return
std::string trim_whitespace(const std::string& str) {
    // Find the first non-whitespace character
    auto first_not_space = std::find_if_not(str.begin(), str.end(), [](unsigned char c){ return std::isspace(c); });
    // If the string is all whitespace, return an empty string
    if (first_not_space == str.end()) {
        return "";
    }
    // Find the last non-whitespace character
    auto last_not_space = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    // Construct the substring from the first non-whitespace to the last non-whitespace character
    return std::string(first_not_space, last_not_space);
}


// Saves configuration to a file.
// - filename: The name of the configuration file to save.
// - data: The ConfigData struct containing the information to save.
// Returns true if saving was successful, false otherwise.
bool save_config(const std::string& filename, const ConfigData& data) {
    std::ofstream outfile(filename); // Opens in truncation mode by default
    if (!outfile.is_open()) {
        std::cerr << "ERROR: Unable to open configuration file (" << filename << ") for writing." << std::endl;
        return false;
    }

    outfile << "ACCOUNT_SID=" << data.account_sid << std::endl;
    outfile << "AUTH_TOKEN=" << data.auth_token << std::endl;
    outfile << "FROM_NUMBER=" << data.from_number << std::endl;

    if (outfile.fail()) {
        std::cerr << "ERROR: Failed to write all data to configuration file (" << filename << ")." << std::endl;
        outfile.close();
        return false;
    }

    outfile.close();
    std::cout << "\nINFO: Configuration saved successfully to " << filename << "." << std::endl;
    return true;
}

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
        std::cerr << "CRITICAL: Memory allocation failed in WriteCallback." << std::endl;
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
// Forward declaration for mocked_send_sms
bool mocked_send_sms(const std::string &account_sid,
                     const std::string &auth_token,
                     const std::string &to_number,
                     const std::string &from_number,
                     const std::string &message_body,
                     std::string &api_response_str);

// Returns true if Twilio indicates success (HTTP 201), false otherwise.
// Returns true if Twilio indicates success (HTTP 201), false otherwise.
bool send_sms(const std::string &account_sid,
              const std::string &auth_token,
              const std::string &to_number,
              const std::string &from_number,
              const std::string &message_body,
              std::string &api_response_str) {

    long current_http_code = 0;
    bool success_status = false;
    api_response_str.clear();

    if (g_test_ctx.test_mode && g_test_ctx.mock_sms_behavior != REAL) {
        success_status = mocked_send_sms(account_sid, auth_token, to_number, from_number, message_body, api_response_str);
        current_http_code = g_test_ctx.mock_response_code; // Mock sets this global for send_sms to retrieve
    } else {
        CURL *curl;
        CURLcode res;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();

        if (curl) {
            std::string url = "https://api.twilio.com/2010-04-01/Accounts/" + account_sid + "/Messages.json";
            std::string post_data = "To=" + url_encode(curl, to_number) +
                                    "&From=" + url_encode(curl, from_number) +
                                    "&Body=" + url_encode(curl, message_body);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            curl_easy_setopt(curl, CURLOPT_USERNAME, account_sid.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, auth_token.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "cpp-sms-app/1.0");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &api_response_str);

            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                // Error message will be printed below using common logging section
                success_status = false;
                // To ensure an error message is printed by common logic if res is not OK:
                // current_http_code can remain 0 or set to a specific internal code,
                // as success_status is false. The curl_easy_strerror will be the main info.
                // For now, we'll let the specific error message be printed by the common block.
                // We need to store the curl error string somewhere or handle it.
                // For simplicity, let's print it here and then common logic will print generic failure.
                 std::cerr << "\nERROR: curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            } else {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &current_http_code);
                success_status = (current_http_code == 201); // Twilio success for SMS creation
            }
            curl_easy_cleanup(curl);
        } else {
            std::cerr << "\nCRITICAL: Failed to initialize libcurl easy handle." << std::endl;
            success_status = false; // Cannot proceed
        }
        curl_global_cleanup();
    }

    // Common logging based on outcome
    std::cout << "\nINFO: HTTP response code from Twilio: " << current_http_code << std::endl;
    std::cout << "INFO: Full API Response from Twilio: " << api_response_str << std::endl;

    if (success_status && current_http_code == 201) { // Ensure both conditions for success message
        std::cout << "\nSUCCESS: SMS successfully queued by Twilio for sending." << std::endl;
    } else if (!success_status) { // Covers general failures or non-201 codes if success_status wasn't true already
        // If it was a libcurl error (res != CURLE_OK), specific error already printed.
        // If it was an HTTP error from Twilio (e.g. 400, 401, 500), this is the place.
        if (current_http_code != 0) { // HTTP error from Twilio
             std::cerr << "\nERROR: SMS sending failed. Twilio responded with HTTP " << current_http_code << "." << std::endl;
             if (g_test_ctx.test_mode && g_test_ctx.mock_sms_behavior != REAL) {
                 // Mock already printed its specific context if it was an error simulation
             } else {
                std::cerr << "INFO: Review the API response above for Twilio-specific error details." << std::endl;
             }
        } else if (res != CURLE_OK && !(g_test_ctx.test_mode && g_test_ctx.mock_sms_behavior == MOCK_PERFORM_FAIL)) {
            // Handled by specific curl error print, this is a fallback.
            // This block might be redundant if specific curl error is always printed above.
        } else if (!(g_test_ctx.test_mode && g_test_ctx.mock_sms_behavior != REAL)) {
             std::cerr << "\nERROR: SMS sending failed due to an unspecified error." << std::endl;
        }
    }
    return success_status;
}


// Mocked version of send_sms for testing purposes
bool mocked_send_sms(const std::string &account_sid,
                     const std::string &auth_token,
                     const std::string &to_number,
                     const std::string &from_number,
                     const std::string &message_body,
                     std::string &api_response_str) {
    api_response_str.clear(); // Start fresh

    switch (g_test_ctx.mock_sms_behavior) {
        case MOCK_SUCCESS:
            std::cout << "MOCK_SEND_SMS: Simulating MOCK_SUCCESS." << std::endl;
            api_response_str = g_test_ctx.mock_api_response_str;
            // g_test_ctx.mock_response_code is already set (default or by directive)
            // Ensure it's a success-like code for this path
            if (g_test_ctx.mock_response_code < 200 || g_test_ctx.mock_response_code > 299) {
                 g_test_ctx.mock_response_code = 201; // Default to 201 if not a success code
                 std::cout << "MOCK_SEND_SMS_INFO: MOCK_SUCCESS forced response code to 201." << std::endl;
            }
            return (g_test_ctx.mock_response_code == 201); // Only true success if 201 for Twilio SMS

        case MOCK_AUTH_FAIL:
            std::cout << "MOCK_SEND_SMS: Simulating MOCK_AUTH_FAIL." << std::endl;
            // If mock_response_code is still its default (201 from MOCK_SUCCESS) or generic success, set to 401
            if (g_test_ctx.mock_response_code >= 200 && g_test_ctx.mock_response_code <= 299) {
                g_test_ctx.mock_response_code = 401;
            }
            // If api_response_str is still default success, set to typical auth error
            if (g_test_ctx.mock_api_response_str.find("SMmockedsuccessfulsid") != std::string::npos && g_test_ctx.mock_api_response_str.length() < 100) {
                api_response_str = "{\"code\": 20003, \"message\": \"Authentication Error - Your AccountSid or AuthToken was incorrect.\", \"more_info\": \"https://www.twilio.com/docs/errors/20003\", \"status\": 401}";
            } else {
                api_response_str = g_test_ctx.mock_api_response_str;
            }
            return false;

        case MOCK_INVALID_TO_FAIL:
            std::cout << "MOCK_SEND_SMS: Simulating MOCK_INVALID_TO_FAIL." << std::endl;
            if (g_test_ctx.mock_response_code >= 200 && g_test_ctx.mock_response_code <= 299) {
                g_test_ctx.mock_response_code = 400;
            }
            if (g_test_ctx.mock_api_response_str.find("SMmockedsuccessfulsid") != std::string::npos && g_test_ctx.mock_api_response_str.length() < 100) {
                api_response_str = "{\"code\": 21211, \"message\": \"The 'To' number is not a valid phone number.\", \"more_info\": \"https://www.twilio.com/docs/errors/21211\", \"status\": 400}";
            } else {
                api_response_str = g_test_ctx.mock_api_response_str;
            }
            return false;

        case MOCK_URL_ENCODE_FAIL:
            std::cout << "MOCK_SEND_SMS: Simulating MOCK_URL_ENCODE_FAIL (leads to generic parameter error)." << std::endl;
             if (g_test_ctx.mock_response_code >= 200 && g_test_ctx.mock_response_code <= 299) {
                g_test_ctx.mock_response_code = 400; // Typically a bad request
            }
            if (g_test_ctx.mock_api_response_str.find("SMmockedsuccessfulsid") != std::string::npos && g_test_ctx.mock_api_response_str.length() < 100) {
                 api_response_str = "{\"code\": 21601, \"message\": \"Invalid Parameter (simulated URL encode fail)\", \"more_info\": \"https://www.twilio.com/docs/errors/21601\", \"status\": 400}";
            } else {
                api_response_str = g_test_ctx.mock_api_response_str;
            }
            return false;

        case MOCK_PERFORM_FAIL:
            std::cout << "MOCK_SEND_SMS: Simulating MOCK_PERFORM_FAIL (e.g., CURLE_COULDNT_CONNECT)." << std::endl;
            g_test_ctx.mock_response_code = 0; // No HTTP response in this case
            api_response_str = ""; // No API response body
            // The actual error message for curl failure will be printed by the main send_sms function's real path recovery.
            // Or, we can simulate the specific cerr message here for consistency in test output if needed.
            // For now, send_sms will print its generic "curl_easy_perform() failed".
            // Let's ensure the common logging in send_sms reflects this.
            // This mock only needs to return false and set code to 0.
            return false;

        default:
            std::cout << "MOCK_SEND_SMS: Unknown g_test_ctx.mock_sms_behavior!" << std::endl;
            g_test_ctx.mock_response_code = 500; // Internal server error
            api_response_str = "{\"code\": 99999, \"message\": \"Internal Mock Error\"}";
            return false;
    }
}


// Validates a phone number string.
// Basic validation: checks if it's not empty and starts with '+'.
// For more robust validation, a regex library or more specific checks would be needed.
// - number: The phone number string to validate.
// Returns true if the number seems valid, false otherwise.
bool is_valid_phone_number(const std::string& number) {
    if (number.empty()) return false;

    if (number[0] != '+') return false;

    // Check length: + followed by 7 to 15 digits. So total length 8 to 16.
    if (number.length() < 8 || number.length() > 16) return false;

    // Check if all characters after '+' are digits
    // std::all_of returns true if the predicate returns true for all elements in the range.
    // Here, it checks characters from the second one (index 1) to the end.
    return std::all_of(number.begin() + 1, number.end(), ::isdigit);
}

// Main function: Entry point of the application.
// Prompts the user for Twilio credentials and SMS details, then calls send_sms.

// Test function for config loading and saving
void run_config_tests() {
    const std::string test_config_file = "test_config_delete_me.txt";
    int tests_passed = 0;
    int tests_failed = 0;

    auto run_test = [&](const std::string& test_name, bool condition) {
        if (condition) {
            std::cout << "Test PASSED: " << test_name << std::endl;
            tests_passed++;
        } else {
            std::cerr << "Test FAILED: " << test_name << std::endl;
            tests_failed++;
        }
    };

    std::cout << "\n--- Running Configuration Tests ---" << std::endl;

    // Test Case 1: Save and Load Cycle (Basic functionality)
    std::cout << "\n--- Test Case 1: Save and Load Cycle (Basic) ---" << std::endl;
    std::remove(test_config_file.c_str()); // Clean before test
    ConfigData original_data;
    original_data.account_sid = "ACtest_sid_123";
    original_data.auth_token = "test_auth_token_456";
    original_data.from_number = "+12345678901";
    original_data.loaded_successfully = true;

    bool save_ok = save_config(test_config_file, original_data);
    run_test("T1.1: Save_config returns true", save_ok);
    if (save_ok) {
        ConfigData loaded_data = load_config(test_config_file);
        run_test("T1.2: Load_config successful after save", loaded_data.loaded_successfully);
        run_test("T1.3: Loaded Account SID matches original", loaded_data.account_sid == original_data.account_sid);
        run_test("T1.4: Loaded Auth Token matches original", loaded_data.auth_token == original_data.auth_token);
        run_test("T1.5: Loaded From Number matches original", loaded_data.from_number == original_data.from_number);
    } else {
        std::cerr << "Skipping load tests for Test Case 1 due to save failure." << std::endl;
        tests_failed += 4; // T1.2 to T1.5
    }

    // Test Case 2: Load Non-Existent File
    std::cout << "\n--- Test Case 2: Load Non-Existent File ---" << std::endl;
    std::remove(test_config_file.c_str());
    ConfigData loaded_non_existent = load_config(test_config_file);
    run_test("T2.1: Load_config on non-existent file sets loaded_successfully to false", !loaded_non_existent.loaded_successfully);

    // Test Case 3: Load Empty File
    std::cout << "\n--- Test Case 3: Load Empty File ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream empty_f(test_config_file); // Create empty file
        empty_f.close();
    }
    ConfigData loaded_empty = load_config(test_config_file);
    run_test("T3.1: Load_config on empty file sets loaded_successfully to false", !loaded_empty.loaded_successfully);
    run_test("T3.2: Loaded Account SID is empty from empty file", loaded_empty.account_sid.empty());
    run_test("T3.3: Loaded Auth Token is empty from empty file", loaded_empty.auth_token.empty());
    run_test("T3.4: Loaded From Number is empty from empty file", loaded_empty.from_number.empty());

    // Test Case 4: Comments and Blank Lines
    std::cout << "\n--- Test Case 4: Comments and Blank Lines ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream comment_file(test_config_file);
        comment_file << "# This is a comment" << std::endl;
        comment_file << "ACCOUNT_SID=ACval_comment" << std::endl;
        comment_file << std::endl; // Blank line
        comment_file << "; Another comment" << std::endl;
        comment_file << "AUTH_TOKEN=token_comment" << std::endl;
        comment_file << "FROM_NUMBER=+12345comment" << std::endl;
        comment_file.close();
    }
    ConfigData loaded_comments = load_config(test_config_file);
    run_test("T4.1: Load_config successful with comments/blank lines", loaded_comments.loaded_successfully);
    run_test("T4.2: Account SID correct with comments/blank lines", loaded_comments.account_sid == "ACval_comment");
    run_test("T4.3: Auth Token correct with comments/blank lines", loaded_comments.auth_token == "token_comment");
    run_test("T4.4: From Number correct with comments/blank lines", loaded_comments.from_number == "+12345comment");

    // Test Case 5: Whitespace in Config Values and Keys
    std::cout << "\n--- Test Case 5: Whitespace in Config Values and Keys ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream ws_file(test_config_file);
        ws_file << " ACCOUNT_SID = ACval_whitespace " << std::endl;
        ws_file << "AUTH_TOKEN= token_whitespace\t" << std::endl;
        ws_file << " FROM_NUMBER =  +12345whitespace  " << std::endl;
        ws_file.close();
    }
    ConfigData loaded_whitespace = load_config(test_config_file);
    run_test("T5.1: Load_config successful with whitespace", loaded_whitespace.loaded_successfully);
    run_test("T5.2: Account SID trimmed correctly", loaded_whitespace.account_sid == "ACval_whitespace");
    run_test("T5.3: Auth Token trimmed correctly", loaded_whitespace.auth_token == "token_whitespace");
    run_test("T5.4: From Number trimmed correctly", loaded_whitespace.from_number == "+12345whitespace");

    // Test Case 6: Case-Insensitive Keys
    std::cout << "\n--- Test Case 6: Case-Insensitive Keys ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream case_file(test_config_file);
        case_file << "account_sid=ACval_case" << std::endl;
        case_file << "Auth_Token=token_case" << std::endl;
        case_file << "from_NUMBER=+12345case" << std::endl;
        case_file.close();
    }
    ConfigData loaded_case = load_config(test_config_file);
    run_test("T6.1: Load_config successful with case-insensitive keys", loaded_case.loaded_successfully);
    run_test("T6.2: Account SID loaded with case-insensitive key", loaded_case.account_sid == "ACval_case");
    run_test("T6.3: Auth Token loaded with case-insensitive key", loaded_case.auth_token == "token_case");
    run_test("T6.4: From Number loaded with case-insensitive key", loaded_case.from_number == "+12345case");

    // Test Case 7: Duplicate Keys (Last one wins)
    std::cout << "\n--- Test Case 7: Duplicate Keys ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream dup_file(test_config_file);
        dup_file << "ACCOUNT_SID=ACfirst" << std::endl;
        dup_file << "AUTH_TOKEN=token_dup1" << std::endl;
        dup_file << "ACCOUNT_SID=ACsecond_dup" << std::endl; // Duplicate SID
        dup_file << "FROM_NUMBER=+12345dup" << std::endl;
        dup_file << "auth_token=token_dup2" << std::endl; // Duplicate token with different case
        dup_file.close();
    }
    ConfigData loaded_dup = load_config(test_config_file);
    run_test("T7.1: Load_config successful with duplicate keys", loaded_dup.loaded_successfully);
    run_test("T7.2: Account SID is the last value for duplicate keys", loaded_dup.account_sid == "ACsecond_dup");
    run_test("T7.3: Auth Token is the last value for duplicate keys (case-insensitive)", loaded_dup.auth_token == "token_dup2");
    run_test("T7.4: From Number is correct with duplicates of other keys", loaded_dup.from_number == "+12345dup");


    // Test Case 8: Load File with Missing FROM_NUMBER
    std::cout << "\n--- Test Case 8: Missing FROM_NUMBER ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream partial_file(test_config_file);
        partial_file << "ACCOUNT_SID=ACpartial_sid" << std::endl;
        partial_file << "AUTH_TOKEN=partial_token" << std::endl;
        partial_file.close();
    }
    ConfigData loaded_partial = load_config(test_config_file);
    run_test("T8.1: Load_config on partial file (missing FROM_NUMBER) sets loaded_successfully to false", !loaded_partial.loaded_successfully);
    run_test("T8.2: Loaded Account SID is empty from partial file", loaded_partial.account_sid.empty());
    run_test("T8.3: Loaded Auth Token is empty from partial file", loaded_partial.auth_token.empty());
    run_test("T8.4: Loaded From Number is empty from partial file", loaded_partial.from_number.empty());

    // Test Case 9: Load File with Malformed Line (no '=') - Existing behavior check
    std::cout << "\n--- Test Case 9: Malformed Line (no '=') ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream malformed_file(test_config_file);
        malformed_file << "ACCOUNT_SID=ACmalformed_sid" << std::endl;
        malformed_file << "AUTH_TOKEN_NO_EQUALS_SIGN" << std::endl; // Malformed line
        malformed_file << "FROM_NUMBER=+19876543210" << std::endl;
        malformed_file.close();
    }
    ConfigData loaded_malformed = load_config(test_config_file);
    run_test("T9.1: Load_config on malformed file (AUTH_TOKEN effectively missing) sets loaded_successfully to false", !loaded_malformed.loaded_successfully);
    run_test("T9.2: Account SID is empty (cleared due to incomplete load)", loaded_malformed.account_sid.empty());
    run_test("T9.3: Auth Token is empty (cleared due to incomplete load)", loaded_malformed.auth_token.empty());
    run_test("T9.4: From Number is empty (cleared due to incomplete load)", loaded_malformed.from_number.empty());

    // Test Case 10: Keys without values (e.g., ACCOUNT_SID=)
    std::cout << "\n--- Test Case 10: Keys without values ---" << std::endl;
    std::remove(test_config_file.c_str());
    {
        std::ofstream no_value_file(test_config_file);
        no_value_file << "ACCOUNT_SID=" << std::endl;
        no_value_file << "AUTH_TOKEN=some_token_for_empty_sid_case" << std::endl;
        no_value_file << "FROM_NUMBER=+12345emptyval" << std::endl;
        no_value_file.close();
    }
    ConfigData loaded_no_value_sid = load_config(test_config_file);
    run_test("T10.1: Load_config sets loaded_successfully to false if ACCOUNT_SID has no value", !loaded_no_value_sid.loaded_successfully);
    run_test("T10.2: Account SID is empty if it had no value", loaded_no_value_sid.account_sid.empty());
    run_test("T10.3: Auth Token is empty (cleared due to incomplete load)", loaded_no_value_sid.auth_token.empty());
    run_test("T10.4: From Number is empty (cleared due to incomplete load)", loaded_no_value_sid.from_number.empty());

    std::remove(test_config_file.c_str()); // Final cleanup
    std::cout << "\n--- Configuration Tests Finished ---" << std::endl;
    std::cout << "Tests Passed: " << tests_passed << ", Tests Failed: " << tests_failed << std::endl;
    if (tests_failed > 0) {
        std::cerr << "THERE WERE TEST FAILURES!" << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
}

// Helper function to process a single line of input, checking for mock directives.
// Returns true if the line was a mock directive, false otherwise.
// If it's a mock directive, global mock variables are updated.
// The `current_line` is passed by reference and might be cleared or modified if it's a mock directive.
bool process_potential_mock_directive(std::string& current_line, TestContext& ctx) {
    if (!ctx.test_mode) return false; // Mock directives only active in test mode

    std::string directive_line = trim_whitespace(current_line);
    std::string value_part;

    if (directive_line.rfind("MOCK_SEND_SMS=", 0) == 0) {
        value_part = directive_line.substr(std::string("MOCK_SEND_SMS=").length());
        value_part = trim_whitespace(value_part);
        if (value_part == "SUCCESS") ctx.mock_sms_behavior = MOCK_SUCCESS;
        else if (value_part == "AUTH_FAIL") ctx.mock_sms_behavior = MOCK_AUTH_FAIL;
        else if (value_part == "INVALID_TO_FAIL") ctx.mock_sms_behavior = MOCK_INVALID_TO_FAIL;
        else if (value_part == "URL_ENCODE_FAIL") ctx.mock_sms_behavior = MOCK_URL_ENCODE_FAIL;
        else if (value_part == "PERFORM_FAIL") ctx.mock_sms_behavior = MOCK_PERFORM_FAIL;
        else if (value_part == "REAL") ctx.mock_sms_behavior = REAL;
        else {
            std::cerr << "TEST_FRAMEWORK_ERROR: Unknown MOCK_SEND_SMS value: " << value_part << std::endl;
            return true;
        }
        std::cout << "INFO_MOCK_UPDATE: g_test_ctx.mock_sms_behavior set to " << value_part << std::endl;
        return true;
    }
    if (directive_line.rfind("MOCK_RESPONSE_CODE=", 0) == 0) {
        value_part = directive_line.substr(std::string("MOCK_RESPONSE_CODE=").length());
        value_part = trim_whitespace(value_part);
        try {
            ctx.mock_response_code = std::stol(value_part);
            std::cout << "INFO_MOCK_UPDATE: g_test_ctx.mock_response_code set to " << ctx.mock_response_code << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "TEST_FRAMEWORK_ERROR: Invalid MOCK_RESPONSE_CODE value: " << value_part << " (" << e.what() << ")" << std::endl;
        }
        return true;
    }
    if (directive_line.rfind("MOCK_API_RESPONSE=", 0) == 0) {
        ctx.mock_api_response_str = directive_line.substr(std::string("MOCK_API_RESPONSE=").length());
        std::cout << "INFO_MOCK_UPDATE: g_test_ctx.mock_api_response_str set to " << ctx.mock_api_response_str << std::endl;
        return true;
    }
    return false;
}

// --- Application Logic Helper Functions ---
static bool setup_test_mode(int argc, char *argv[], TestContext& ctx) {
    if (argc == 3 && std::string(argv[1]) == "--test-mode") {
        ctx.test_mode = true;
        ctx.scenario_filepath = argv[2];
        // This initial cout will go to console, as redirection is set up after this.
        std::cout << "INFO: Running in Test Mode with scenario: " << ctx.scenario_filepath << std::endl;

        ctx.scenario_stream.open(ctx.scenario_filepath);
        if (!ctx.scenario_stream.is_open()) {
            std::cerr << "CRITICAL: Failed to open scenario file: " << ctx.scenario_filepath << std::endl;
            return false;
        }
        ctx.input_stream = &ctx.scenario_stream;

        std::string base_filepath = ctx.scenario_filepath;
        size_t last_dot = base_filepath.find_last_of(".");
        if (last_dot != std::string::npos) {
            base_filepath = base_filepath.substr(0, last_dot);
        }
        std::string out_filepath = base_filepath + ".out";
        std::string err_filepath = base_filepath + ".err";

        ctx.redirect_out_file.open(out_filepath);
        if (!ctx.redirect_out_file.is_open()) {
            std::cerr << "CRITICAL: Failed to open output file for cout redirection: " << out_filepath << std::endl;
            return false;
        }
        ctx.orig_cout_buf = std::cout.rdbuf();
        std::cout.rdbuf(ctx.redirect_out_file.rdbuf());

        ctx.redirect_err_file.open(err_filepath);
        if (!ctx.redirect_err_file.is_open()) {
            if(ctx.orig_cout_buf) std::cout.rdbuf(ctx.orig_cout_buf);
            std::cout << "CRITICAL: Failed to open error file for cerr redirection: " << err_filepath << std::endl;
             if(ctx.orig_cout_buf) std::cout.rdbuf(ctx.redirect_out_file.rdbuf()); // Re-redirect if possible
            return false;
        }
        ctx.orig_cerr_buf = std::cerr.rdbuf();
        std::cerr.rdbuf(ctx.redirect_err_file.rdbuf());

        std::cout << "INFO: Standard output redirected to: " << out_filepath << std::endl;
        std::cerr << "INFO: Standard error redirected to: " << err_filepath << std::endl;
        return true;
    }
    return true; // Not in test mode, but setup is "successful" for normal operation
}

static void teardown_test_mode(TestContext& ctx) {
    if (ctx.test_mode) {
        if (ctx.orig_cout_buf) {
            std::cout.rdbuf(ctx.orig_cout_buf);
            if(ctx.redirect_out_file.is_open()) ctx.redirect_out_file.close();
        }
        if (ctx.orig_cerr_buf) {
            std::cerr.rdbuf(ctx.orig_cerr_buf);
             if(ctx.redirect_err_file.is_open()) ctx.redirect_err_file.close();
        }
        if (ctx.scenario_stream.is_open()) {
            ctx.scenario_stream.close();
        }
        std::cout << "INFO: Test Mode finished. Restored cout/cerr." << std::endl;
    }
}

static void get_user_choice_for_loaded_config(const ConfigData& loaded_config, ConfigData& current_config, TestContext& ctx) {
    if (loaded_config.loaded_successfully) {
        std::cout << "--- Loaded Configuration ---" << std::endl;
        std::cout << "Account SID: " << loaded_config.account_sid << std::endl;
        std::cout << "Auth Token:  " << mask_auth_token(loaded_config.auth_token) << std::endl;
        std::cout << "From Number: " << loaded_config.from_number << std::endl << std::endl;

        char use_loaded_choice = 'n';
        std::string choice_str_loaded_creds;
        while(true) {
            std::cout << "Do you want to use these loaded credentials? (Y/N, default N): ";
            do {
                std::getline(*ctx.input_stream, choice_str_loaded_creds);
                if (!ctx.input_stream->good()) {
                    std::cerr << "\nCRITICAL: Input stream error or EOF reached. Exiting application." << std::endl;
                    exit(EXIT_FAILURE);
                }
            } while (process_potential_mock_directive(choice_str_loaded_creds, ctx) && ctx.input_stream->good());
            choice_str_loaded_creds = trim_whitespace(choice_str_loaded_creds);
            if (choice_str_loaded_creds.empty() || choice_str_loaded_creds[0] == 'n' || choice_str_loaded_creds[0] == 'N') {
                use_loaded_choice = 'n';
                break;
            }
            if (choice_str_loaded_creds[0] == 'y' || choice_str_loaded_creds[0] == 'Y') {
                use_loaded_choice = 'y';
                break;
            }
            std::cout << "INFO: Invalid choice. Please enter 'Y' or 'N'." << std::endl;
        }

        if (use_loaded_choice == 'y') {
            current_config = loaded_config;
            current_config.loaded_successfully = true;
            std::cout << "\nINFO: Using loaded credentials." << std::endl;
        } else {
            std::cout << "\nINFO: Loaded credentials will not be used. You will be prompted to enter them manually." << std::endl;
        }
    } else {
        // Error/Info message already printed by load_config
    }
}

static void collect_credentials_interactively(ConfigData& current_config, const ConfigData& loaded_config, TestContext& ctx) {
    std::cout << "\n--- Enter Twilio Credentials ---" << std::endl;
    std::string input_sid, input_token, input_from_number;

    // Account SID
    while (true) {
        std::cout << "Enter Twilio Account SID";
        if (current_config.loaded_successfully && !current_config.account_sid.empty()) {
             std::cout << " (loaded: " << current_config.account_sid << ", press Enter to use this): ";
        } else if (!loaded_config.account_sid.empty() && !current_config.loaded_successfully) {
             std::cout << " (available from " << CONFIG_FILENAME << ": " << loaded_config.account_sid << ", or enter new): ";
        } else { std::cout << ": "; }
        do { std::getline(*ctx.input_stream, input_sid); if (!ctx.input_stream->good()) {std::cerr << "\nCRITICAL: EOF SID" << std::endl; exit(EXIT_FAILURE);}} while (process_potential_mock_directive(input_sid, ctx) && ctx.input_stream->good());
        input_sid = trim_whitespace(input_sid);
        if (input_sid.empty()) {
            if (current_config.loaded_successfully && !current_config.account_sid.empty()) break;
            if (current_config.account_sid.empty()){ std::cerr << "ERROR: Account SID cannot be empty..." << std::endl; continue;}
        } else { current_config.account_sid = input_sid; current_config.loaded_successfully = false;}
        if (!current_config.account_sid.empty() && (current_config.account_sid.rfind("AC", 0) == 0) && current_config.account_sid.length() >= 34) break;
        std::cerr << "ERROR: Invalid Account SID..." << std::endl;
    }

    // Auth Token
    while (true) {
        std::cout << "Enter Twilio Auth Token";
        if (current_config.loaded_successfully && !current_config.auth_token.empty()) {
             std::cout << " (loaded: " << mask_auth_token(current_config.auth_token) << ", press Enter to use this): ";
        } else if (!loaded_config.auth_token.empty() && !current_config.loaded_successfully && !current_config.account_sid.empty() && loaded_config.account_sid == current_config.account_sid) {
             std::cout << " (available from " << CONFIG_FILENAME << "...: " << mask_auth_token(loaded_config.auth_token) << ", or enter new): ";
        } else { std::cout << ": ";}
        do { std::getline(*ctx.input_stream, input_token); if (!ctx.input_stream->good()) {std::cerr << "\nCRITICAL: EOF Token" << std::endl; exit(EXIT_FAILURE);}} while (process_potential_mock_directive(input_token, ctx) && ctx.input_stream->good());
        input_token = trim_whitespace(input_token);
        if (input_token.empty()) {
            if (current_config.loaded_successfully && !current_config.auth_token.empty()) break;
            std::cerr << "ERROR: Auth Token cannot be empty..." << std::endl; continue;
        } else { current_config.auth_token = input_token; current_config.loaded_successfully = false;}
        if (!current_config.auth_token.empty()) break;
        std::cerr << "ERROR: Auth Token cannot be empty." << std::endl;
    }

    // From Number
    while (true) {
        std::cout << "Enter your Twilio Phone Number (From Number)";
        if (current_config.loaded_successfully && !current_config.from_number.empty()) {
             std::cout << " (loaded: " << current_config.from_number << ", press Enter to use this): ";
        } else if (!loaded_config.from_number.empty() && !current_config.loaded_successfully && !current_config.account_sid.empty() && loaded_config.account_sid == current_config.account_sid) {
            std::cout << " (available from " << CONFIG_FILENAME << "...: " << loaded_config.from_number << ", or enter new): ";
        } else { std::cout << ": ";}
        do { std::getline(*ctx.input_stream, input_from_number); if(!ctx.input_stream->good()){std::cerr << "\nCRITICAL: EOF FromNumber"<<std::endl; exit(EXIT_FAILURE);}} while(process_potential_mock_directive(input_from_number, ctx) && ctx.input_stream->good());
        input_from_number = trim_whitespace(input_from_number);
        if (input_from_number.empty()) {
            if (current_config.loaded_successfully && !current_config.from_number.empty()) break;
            std::cerr << "ERROR: Twilio Phone Number cannot be empty..." << std::endl; continue;
        } else { current_config.from_number = input_from_number; current_config.loaded_successfully = false;}
        if (is_valid_phone_number(current_config.from_number)) break;
        std::cerr << "ERROR: Invalid Twilio Phone Number format..." << std::endl;
    }
    if (!current_config.loaded_successfully) {
        if (!current_config.account_sid.empty() && !current_config.auth_token.empty() && !current_config.from_number.empty()) {
            current_config.loaded_successfully = true;
        }
    }
}

static void collect_sms_details_interactively(std::string& to_number, std::string& message_body, TestContext& ctx) {
    std::cout << "\n--- Enter Recipient And Message ---" << std::endl;
    while (true) {
        std::cout << "Enter Recipient's Phone Number (E.164 format, e.g., +1234567890): ";
        do {std::getline(*ctx.input_stream, to_number); if(!ctx.input_stream->good()){std::cerr << "\nCRITICAL: EOF ToNumber"<<std::endl; exit(EXIT_FAILURE);}} while(process_potential_mock_directive(to_number, ctx) && ctx.input_stream->good());
        to_number = trim_whitespace(to_number);
        if (is_valid_phone_number(to_number)) break;
        std::cerr << "ERROR: Invalid recipient phone number format..." << std::endl;
    }
    std::cout << "Enter Message Body: ";
    do {std::getline(*ctx.input_stream, message_body); if(!ctx.input_stream->good()){std::cerr << "\nCRITICAL: EOF MsgBody"<<std::endl; exit(EXIT_FAILURE);}} while(process_potential_mock_directive(message_body, ctx) && ctx.input_stream->good());
    message_body = trim_whitespace(message_body);
    if (message_body.empty()) {
        std::cout << "WARNING: Message body is empty. An empty SMS will be sent." << std::endl;
    }
}

static void prompt_and_save_config_if_needed(const ConfigData& current_config, TestContext& ctx) {
    if (current_config.loaded_successfully) {
        char save_choice = 'N';
        std::cout << "\n--- Save Configuration ---" << std::endl;
        std::cout << "Do you want to save ... to '" << CONFIG_FILENAME << "' ...? (Y/N, default N): ";
        std::string choice_str_save_cfg;
        do {std::getline(*ctx.input_stream, choice_str_save_cfg); if(!ctx.input_stream->good()){std::cerr << "\nCRITICAL: EOF SaveChoice"<<std::endl; exit(EXIT_FAILURE);}} while(process_potential_mock_directive(choice_str_save_cfg, ctx) && ctx.input_stream->good());
        choice_str_save_cfg = trim_whitespace(choice_str_save_cfg);
        if (!choice_str_save_cfg.empty() && (choice_str_save_cfg[0] == 'y' || choice_str_save_cfg[0] == 'Y')) {
            save_choice = 'Y';
        }
        if (save_choice == 'Y') {
            save_config(CONFIG_FILENAME, current_config);
        } else {
            std::cout << "\nINFO: Configuration will not be saved." << std::endl;
        }
    } else {
        std::cout << "\nINFO: Current configuration is incomplete; skipping save option." << std::endl;
    }
}

int main(int argc, char *argv[]) {
    if (!setup_test_mode(argc, argv, g_test_ctx)) {
        return EXIT_FAILURE;
    }

    // run_config_tests(); // Keep this commented unless specifically running unit-like tests for config

    ConfigData loaded_config = load_config(CONFIG_FILENAME);
    ConfigData current_config;
    std::string to_number, message_body, api_response;

    std::cout << "--- C++ SMS Sender using Twilio ---" << std::endl << std::endl;

    get_user_choice_for_loaded_config(loaded_config, current_config, g_test_ctx);
    collect_credentials_interactively(current_config, loaded_config, g_test_ctx);
    collect_sms_details_interactively(to_number, message_body, g_test_ctx);
    prompt_and_save_config_if_needed(current_config, g_test_ctx);

    std::cout << "\n--- Sending SMS ---" << std::endl;
    std::cout << "INFO: Attempting to send SMS via Twilio..." << std::endl;
    if (send_sms(current_config.account_sid, current_config.auth_token, to_number, current_config.from_number, message_body, api_response)) {
        // Messages handled by send_sms
    } else {
        // Error messages handled by send_sms or sub-functions
        // For clarity, a general error message might be useful if not already covered comprehensively
        // std::cerr << "ERROR: Overall message sending process failed. Check previous messages for details." << std::endl;
    }

    teardown_test_mode(g_test_ctx);
   return 0;

}
