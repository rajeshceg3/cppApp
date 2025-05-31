# C++ SMS Sender/Receiver using Twilio

## Description
This is a command-line C++ application with dual functionality:
1.  **Send Mode:** Allows users to send SMS messages via the Twilio API by providing credentials and message details through CLI prompts.
2.  **Receive Mode:** Runs as an HTTP server to receive incoming SMS messages from Twilio via webhooks and displays their content (From number and Body) on the console.

## Prerequisites
To build and run this application, you will need:
- A C++ compiler that supports C++17 (e.g., g++).
- `make` build automation tool.
- Development libraries:
    - `libcurl` (e.g., `libcurl4-openssl-dev` on Debian/Ubuntu) - for sending SMS.
    - `libmicrohttpd` (e.g., `libmicrohttpd-dev` on Debian/Ubuntu) - for the webhook server in receive mode.
- A Twilio account:
    - Account SID
    - Auth Token
    - A Twilio phone number (capable of sending and/or receiving SMS)

## Building the Application
1.  Navigate to the project's root directory (where the `Makefile` is located).
2.  Clean any previous builds and compile the application:
    ```bash
    make clean && make
    ```
    This will create an executable file named `sms_app` inside the `build` directory.

## Modes of Operation

### Receive Mode (Default Webhook Server)
-   **Command:** `./build/sms_app`
-   **Functionality:** Starts an HTTP server on port 8080. It listens for incoming SMS messages POSTed by Twilio to the `/twilio_webhook` path (though the current server implementation responds to POST on any path).
-   **Console Output:** When a message is received via the webhook, the "From" number and "Body" of the SMS will be printed to the console. Server start and stop messages, and basic request information (URL, method) are also logged.

### Send Mode (CLI for Sending SMS)
-   **Command:** `./build/sms_app --send`
-   **Functionality:** Runs a command-line interface that prompts you to enter:
    *   Twilio Account SID
    *   Twilio Auth Token
    *   Recipient's phone number (e.g., `+12223334444`)
    *   Your Twilio phone number (e.g., `+15558675309`)
    *   The message to send.
-   **Console Output:** Shows prompts for input, and then the HTTP status code and API response from Twilio after attempting to send the SMS.

## Configuring Twilio for Receiving Messages (Webhook)
To use the **Receive Mode**, Twilio needs to know where to send incoming SMS messages. This involves making your server publicly accessible and configuring the webhook in your Twilio console.

1.  **Expose your local server (for development/testing):**
    *   The server runs on `http://localhost:8080`, which is not accessible from the public internet.
    *   Tools like `ngrok` can create a secure tunnel to your local server.
        *   Install `ngrok` from [https://ngrok.com/](https://ngrok.com/).
        *   Run it to expose your local port 8080:
            ```bash
            ngrok http 8080
            ```
        *   `ngrok` will display a public URL (e.g., `https://<unique-id>.ngrok-free.app` or similar). Copy this URL. **Note:** The free version of ngrok provides a temporary URL that changes each time you restart it.

2.  **Configure Twilio Phone Number:**
    *   Log in to your Twilio console.
    *   Navigate to the settings for the Twilio phone number you want to use for receiving messages. This is usually under "Phone Numbers" -> "Manage" -> "Active Numbers", then click on the number.
    *   Scroll down to the "Messaging" section.
    *   Find the setting for "A MESSAGE COMES IN".
    *   Set it to "Webhook".
    *   In the URL field, paste your public `ngrok` URL and append `/twilio_webhook` (or your desired endpoint path, though the current server code handles any POST path). For example:
        `https://<your_ngrok_forwarding_url>/twilio_webhook`
    *   Set the HTTP method to `HTTP POST`.
    *   Save the configuration.

Now, when an SMS is sent to your Twilio number, Twilio will forward the message data (like "From" and "Body") to your application's webhook endpoint, and you should see it logged in your server's console.

## Error Handling

### Send Mode:
- If libcurl encounters an issue making the HTTP request (e.g., network problems), it will print an error message.
- If the Twilio API returns an error (e.g., authentication failure, invalid phone number), the application will display the HTTP status code and the JSON error response received from Twilio.
- Input validation for credentials and phone numbers is performed; you'll be re-prompted if input is invalid.

### Receive Mode:
- Server startup errors (e.g., port 8080 already in use) are printed to `std::cerr`, and the application will exit.
- Errors during webhook request processing (e.g., failure to create a post processor) are logged to `std::cerr`, and an HTTP 500 "Internal Server Error" response is sent to Twilio.
- Basic request information (URL, method) and parsed message data ("From", "Body") are logged to `std::cout`. If "From" or "Body" are not found in a POST request, a message indicating this is logged.
```
