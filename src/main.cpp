
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
        std::cout << "Info: Configuration file (" << filename << ") not found or cannot be opened. Please enter details manually." << std::endl;
        config.loaded_successfully = false;
        return config;
    }

    std::string line;
    int fields_found = 0;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            if (key == "ACCOUNT_SID") {
                config.account_sid = value;
                fields_found++;
            } else if (key == "AUTH_TOKEN") {
                config.auth_token = value;
                fields_found++;
            } else if (key == "FROM_NUMBER") {
                config.from_number = value;
                fields_found++;
            }
        }
    }
    infile.close();

    // Check if all essential fields were found
    if (fields_found >= 3 && !config.account_sid.empty() && !config.auth_token.empty() && !config.from_number.empty()) {
        config.loaded_successfully = true;
        std::cout << "Info: Configuration loaded successfully from " << filename << std::endl;
    } else {
        config.loaded_successfully = false;
        std::cout << "Warning: Configuration file (" << filename << ") is incomplete or malformed. Please enter details manually." << std::endl;
        // Reset potentially partially loaded data to ensure a clean state
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

// Saves configuration to a file.
// - filename: The name of the configuration file to save.
// - data: The ConfigData struct containing the information to save.
// Returns true if saving was successful, false otherwise.
bool save_config(const std::string& filename, const ConfigData& data) {
    std::ofstream outfile(filename); // Opens in truncation mode by default
    if (!outfile.is_open()) {
        std::cerr << "Error: Unable to open configuration file (" << filename << ") for writing." << std::endl;
        return false;
    }

    outfile << "ACCOUNT_SID=" << data.account_sid << std::endl;
    outfile << "AUTH_TOKEN=" << data.auth_token << std::endl;
    outfile << "FROM_NUMBER=" << data.from_number << std::endl;

    if (outfile.fail()) {
        std::cerr << "Error: Failed to write all data to configuration file (" << filename << ")." << std::endl;
        outfile.close();
        return false;
    }

    outfile.close();
    std::cout << "Info: Configuration saved successfully to " << filename << std::endl;
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

    // Test Case 1: Save and Load Cycle
    std::cout << "\n--- Test Case 1: Save and Load Cycle ---" << std::endl;
    ConfigData original_data;
    original_data.account_sid = "ACtest_sid_123";
    original_data.auth_token = "test_auth_token_456";
    original_data.from_number = "+12345678901";
    original_data.loaded_successfully = true; // Mark as valid for saving

    bool save_ok = save_config(test_config_file, original_data);
    run_test("Save_config returns true", save_ok);

    if (save_ok) {
        ConfigData loaded_data = load_config(test_config_file);
        run_test("Load_config successful after save", loaded_data.loaded_successfully);
        run_test("Loaded Account SID matches original", loaded_data.account_sid == original_data.account_sid);
        run_test("Loaded Auth Token matches original", loaded_data.auth_token == original_data.auth_token);
        run_test("Loaded From Number matches original", loaded_data.from_number == original_data.from_number);
    } else {
        std::cerr << "Skipping load tests for Test Case 1 due to save failure." << std::endl;
        tests_failed += 4; // Count the skipped assertions as failed for consistency in reporting
    }

    // Test Case 2: Load Non-Existent File
    std::cout << "\n--- Test Case 2: Load Non-Existent File ---" << std::endl;
    std::remove(test_config_file.c_str()); // Ensure file does not exist
    ConfigData loaded_non_existent = load_config(test_config_file);
    // load_config prints "Info: Configuration file (...) not found..."
    run_test("Load_config on non-existent file sets loaded_successfully to false", !loaded_non_existent.loaded_successfully);

    // Test Case 3: Load Empty File
    std::cout << "\n--- Test Case 3: Load Empty File ---" << std::endl;
    std::ofstream empty_file(test_config_file); // Create empty file
    empty_file.close();
    ConfigData loaded_empty = load_config(test_config_file);
    // load_config prints "Warning: Configuration file (...) is incomplete or malformed..."
    run_test("Load_config on empty file sets loaded_successfully to false", !loaded_empty.loaded_successfully);
    run_test("Loaded Account SID is empty from empty file", loaded_empty.account_sid.empty());
    run_test("Loaded Auth Token is empty from empty file", loaded_empty.auth_token.empty());
    run_test("Loaded From Number is empty from empty file", loaded_empty.from_number.empty());


    // Test Case 4: Load File with Missing Keys
    std::cout << "\n--- Test Case 4: Load File with Missing Keys ---" << std::endl;
    std::ofstream partial_file(test_config_file);
    partial_file << "ACCOUNT_SID=ACpartial_sid" << std::endl;
    partial_file.close();
    ConfigData loaded_partial = load_config(test_config_file);
    // load_config prints "Warning: Configuration file (...) is incomplete or malformed..."
    run_test("Load_config on partial file (missing token, from_number) sets loaded_successfully to false", !loaded_partial.loaded_successfully);
    // Current load_config clears fields if not all are present
    run_test("Loaded Account SID is empty from partial file (due to current clearing behavior)", loaded_partial.account_sid.empty());
    run_test("Loaded Auth Token is empty from partial file", loaded_partial.auth_token.empty());
    run_test("Loaded From Number is empty from partial file", loaded_partial.from_number.empty());


    // Test Case 5: Load File with Malformed Line (e.g. no '=')
    std::cout << "\n--- Test Case 5: Load File with Malformed Line ---" << std::endl;
    std::ofstream malformed_file(test_config_file);
    malformed_file << "ACCOUNT_SID=ACmalformed_sid" << std::endl;
    malformed_file << "AUTH_TOKEN_NO_EQUALS_SIGN" << std::endl; // Malformed line
    malformed_file << "FROM_NUMBER=+19876543210" << std::endl;
    malformed_file.close();
    ConfigData loaded_malformed = load_config(test_config_file);
    // load_config prints "Warning: Configuration file (...) is incomplete or malformed..." because AUTH_TOKEN will be missing
    run_test("Load_config on malformed file (missing AUTH_TOKEN) sets loaded_successfully to false", !loaded_malformed.loaded_successfully);
    // Current load_config clears fields if not all are present
    run_test("Loaded Account SID is empty from malformed file (due to current clearing behavior)", loaded_malformed.account_sid.empty());
    run_test("Loaded Auth Token is empty from malformed file", loaded_malformed.auth_token.empty());
    run_test("Loaded From Number is empty from malformed file (due to current clearing behavior)", loaded_malformed.from_number.empty());

    // Cleanup
    std::remove(test_config_file.c_str());
    std::cout << "\n--- Configuration Tests Finished ---" << std::endl;
    std::cout << "Tests Passed: " << tests_passed << ", Tests Failed: " << tests_failed << std::endl;
    if (tests_failed > 0) {
        std::cerr << "THERE WERE TEST FAILURES!" << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
}


int main() {
    // run_config_tests(); // Uncomment to run configuration tests

    ConfigData loaded_config = load_config(CONFIG_FILENAME);
    ConfigData current_config; // Holds the actual values to be used or saved

    std::string to_number; // Recipient number, not saved in config
    std::string message_body;
    std::string api_response;

    std::cout << "--- C++ SMS Sender using Twilio ---" << std::endl;

    if (loaded_config.loaded_successfully) {
        std::cout << "\n--- Loaded Configuration ---" << std::endl;
        std::cout << "Account SID: " << loaded_config.account_sid << std::endl;
        std::cout << "Auth Token: " << mask_auth_token(loaded_config.auth_token) << std::endl;
        std::cout << "From Number: " << loaded_config.from_number << std::endl;

        char use_loaded_choice = 'n';
        std::cout << "Do you want to use these loaded credentials? (y/n, default n): ";
        std::string choice_str;
        std::getline(std::cin, choice_str);
        if (!choice_str.empty() && (choice_str[0] == 'y' || choice_str[0] == 'Y')) {
            use_loaded_choice = 'y';
        }

        if (use_loaded_choice == 'y') {
            current_config = loaded_config;
            current_config.loaded_successfully = true; // Mark as successfully "loaded" into current_config
            std::cout << "Info: Using loaded credentials." << std::endl;
        } else {
            std::cout << "Info: Loaded credentials will not be used. Please enter details manually." << std::endl;
            // current_config remains empty, loaded_successfully defaults to false
        }
    } else {
        // No successful load, current_config remains empty, loaded_successfully defaults to false
    }
    std::cout << "\n--- Enter SMS Details ---" << std::endl;

    // Input loop for Account SID
    while (true) {
        std::cout << "Enter your Twilio Account SID";
        if (current_config.loaded_successfully && !current_config.account_sid.empty()) { // Value might be from loaded_config
             std::cout << " (loaded: " << current_config.account_sid << ", press Enter to use): ";
        } else if (!loaded_config.account_sid.empty() && !current_config.loaded_successfully) { // Loaded but user opted out
             std::cout << " (previously loaded: " << loaded_config.account_sid << "): ";
        } else {
            std::cout << ": ";
        }
        std::string input_sid;
        std::getline(std::cin, input_sid);

        if (input_sid.empty()) {
            if (current_config.loaded_successfully && !current_config.account_sid.empty()) {
                // User pressed Enter to use the SID already in current_config (from loaded)
                // No change needed to current_config.account_sid, validation already passed implicitly
                break;
            } else if (!current_config.loaded_successfully && !loaded_config.account_sid.empty()) {
                 // User pressed Enter, but didn't opt to use loaded config.
                 // If they now want to use the one they saw as "previously loaded", they should type it.
                 // Or, if loaded_config.account_sid was empty, this path means they entered nothing for a fresh field.
                 // This case could be handled by re-prompting or taking loaded_config.account_sid if user confirms.
                 // For simplicity, if they opted out of using loaded config, and then press enter for SID,
                 // we assume they mean to use the "previously loaded" one *if it exists*.
                 // This is a bit ambiguous, better to make them re-type if they opted out.
                 // Let's stick to: if they opted out, they must provide new input.
                 // If current_config.account_sid is empty (because not loaded or user opted out), input_sid must not be empty.
                 if (current_config.account_sid.empty()){ // True if not loaded or user opted out
                    std::cerr << "Account SID cannot be empty. Please enter a valid Account SID." << std::endl;
                    continue;
                 }
            } else { // No loaded SID available or they opted out, and they entered nothing.
                 std::cerr << "Account SID cannot be empty. Please enter a valid Account SID." << std::endl;
                 continue;
            }
        } else { // User typed something
            current_config.account_sid = input_sid;
        }

        // Validate the SID (whether newly typed or taken from loaded)
        if (!current_config.account_sid.empty() && (current_config.account_sid.rfind("AC", 0) == 0) && current_config.account_sid.length() > 30) {
            current_config.loaded_successfully = true; // Mark that we have a valid SID now, even if manually entered
            break;
        }
        current_config.loaded_successfully = false; // SID is invalid, so current_config is not "successful"
        std::cerr << "Invalid Account SID format. It should start with 'AC', be non-empty, and typically >30 chars." << std::endl;
    }

    // Input loop for Auth Token
    while (true) {
        std::cout << "Enter your Twilio Auth Token";
        if (current_config.loaded_successfully && !current_config.auth_token.empty()) {
            std::cout << " (loaded: " << mask_auth_token(current_config.auth_token) << ", press Enter to use): ";
        } else if (!loaded_config.auth_token.empty() && !current_config.loaded_successfully && !current_config.account_sid.empty() && loaded_config.account_sid == current_config.account_sid) {
            // This complex condition tries to see if they are entering details for the same account as was loaded but opted out.
            // Simpler: if current_config.auth_token is empty, they must type.
             std::cout << " (previously loaded: " << mask_auth_token(loaded_config.auth_token) << "): ";
        }
        else {
            std::cout << ": ";
        }
        std::string input_token;
        std::getline(std::cin, input_token);

        if (input_token.empty()) {
            if (current_config.loaded_successfully && !current_config.auth_token.empty()) {
                break; // Use existing token in current_config
            } else {
                 std::cerr << "Auth Token cannot be empty if not using a loaded value. Please enter a token." << std::endl;
                 continue;
            }
        } else {
            current_config.auth_token = input_token;
        }

        if (!current_config.auth_token.empty()) {
            break;
        }
        std::cerr << "Auth Token cannot be empty." << std::endl; // Should be caught by previous checks
    }

    // Input loop for Twilio Phone Number (From Number)
    while (true) {
        std::cout << "Enter your Twilio phone number";
        if (current_config.loaded_successfully && !current_config.from_number.empty()) {
            std::cout << " (loaded: " << current_config.from_number << ", press Enter to use): ";
        } else if (!loaded_config.from_number.empty() && !current_config.loaded_successfully && !current_config.account_sid.empty() && loaded_config.account_sid == current_config.account_sid) {
             std::cout << " (previously loaded: " << loaded_config.from_number << "): ";
        }
        else {
            std::cout << ": ";
        }
        std::string input_from_number;
        std::getline(std::cin, input_from_number);

        if (input_from_number.empty()) {
            if (current_config.loaded_successfully && !current_config.from_number.empty()) {
                break; // Use existing from_number in current_config
            } else {
                std::cerr << "Twilio phone number cannot be empty if not using a loaded value. Please enter a number." << std::endl;
                continue;
            }
        } else {
            current_config.from_number = input_from_number;
        }

        if (is_valid_phone_number(current_config.from_number)) {
            break;
        }
        std::cerr << "Invalid Twilio phone number format. It must start with '+' and be non-empty (e.g., +10987654321)." << std::endl;
    }

    // Input for Recipient's phone number (not from config)
    while (true) {
        std::cout << "Enter the recipient's phone number (e.g., +1234567890): ";
        std::getline(std::cin, to_number);
        if (is_valid_phone_number(to_number)) {
            break;
        }
        std::cerr << "Invalid recipient phone number format. It must start with '+' and be non-empty (e.g., +1234567890)." << std::endl;
    }

    // Get the message body from the user
    std::cout << "Enter the message body: ";
    std::getline(std::cin, message_body);
    if (message_body.empty()) {
        std::cout << "Warning: Message body is empty." << std::endl;
    }

    // Prompt to save configuration
    // Only ask if the current_config is valid (all fields populated)
    if (!current_config.account_sid.empty() && !current_config.auth_token.empty() && !current_config.from_number.empty()) {
        char save_choice = 'n';
        std::cout << "\nDo you want to save the current Twilio Account SID, Auth Token, and From Number to "
                  << CONFIG_FILENAME << "? (y/n, default n): ";
        std::string choice_str;
        std::getline(std::cin, choice_str);
        if (!choice_str.empty() && (choice_str[0] == 'y' || choice_str[0] == 'Y')) {
            save_choice = 'y';
        }

        if (save_choice == 'y') {
            // Ensure loaded_successfully is true if we're saving it,
            // as it represents a complete, usable set of credentials.
            current_config.loaded_successfully = true;
            save_config(CONFIG_FILENAME, current_config);
        } else {
            std::cout << "Info: Configuration will not be saved." << std::endl;
        }
    } else {
        std::cout << "\nInfo: Current configuration is incomplete, skipping save option." << std::endl;
    }

    // Attempt to send the SMS
    std::cout << "\nSending SMS..." << std::endl;
    if (send_sms(current_config.account_sid, current_config.auth_token, to_number, current_config.from_number, message_body, api_response)) {
        std::cout << "Message sending process completed successfully based on API response." << std::endl;
    } else {
        std::cerr << "Message sending process failed or API indicated an error." << std::endl;
    }

   return 0;

}
