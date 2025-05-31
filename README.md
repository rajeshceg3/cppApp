# C++ SMS Sender using Twilio

## Description
This is a simple command-line C++ application that allows users to send SMS messages via the Twilio API. It prompts the user for their Twilio credentials, recipient and sender phone numbers, and the message content.

## Prerequisites
To build and run this application, you will need:
- A C++ compiler that supports C++11 (e.g., g++).
- `make` build automation tool.
- `libcurl` development library.
    - On Debian/Ubuntu: `sudo apt-get install libcurl4-openssl-dev`
    - On Fedora/CentOS/RHEL: `sudo yum install libcurl-devel`
    - On macOS (using Homebrew): `brew install curl`
- A Twilio account:
    - Account SID
    - Auth Token
    - A Twilio phone number (capable of sending SMS)

## Building the Application
1.  Navigate to the project's root directory (where the `Makefile` is located).
2.  Clean any previous builds and compile the application:
    ```bash
    make clean && make
    ```
    This will create an executable file named `sms_app` inside the `build` directory.

## Running the Application
1.  Execute the application from the project root:
    ```bash
    ./build/sms_app
    ```
2.  The application will then prompt you to enter the following information:
    *   **Twilio Account SID:** Your unique identifier for your Twilio account (starts with `AC...`).
    *   **Twilio Auth Token:** Your secret token for authenticating with the Twilio API.
    *   **Recipient's phone number:** The phone number you want to send an SMS to (must be in E.164 format, e.g., `+12223334444`).
    *   **Your Twilio phone number:** One of your Twilio phone numbers that is enabled for sending SMS (must be in E.164 format, e.g., `+15558675309`).
    *   **The message to send:** The content of the SMS message.

## Example Usage
Here's what a typical session might look like:

```
./build/sms_app
--- C++ SMS Sender using Twilio ---
Enter your Twilio Account SID: ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Enter your Twilio Auth Token: yoursecrettokenxxxxxxxxxxxxxxx
Enter the recipient's phone number (e.g., +1234567890): +1RecipientPhoneNumber
Enter your Twilio phone number (e.g., +10987654321): +1YourTwilioPhoneNumber
Enter the message body: Hello from my C++ SMS application!

Sending SMS...
HTTP response code: 201
API Response: {"sid": "SMxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", "date_created": "...", ...}
SMS successfully queued by Twilio.
Message sending process completed successfully based on API response.
```
*(The exact API response will vary.)*

## Error Handling
- If libcurl encounters an issue making the HTTP request (e.g., network problems), it will print an error message.
- If the Twilio API returns an error (e.g., authentication failure, invalid phone number), the application will display the HTTP status code and the JSON error response received from Twilio. For example, an authentication error might show:
  ```
  HTTP response code: 401
  API Response: {"code": 20003, "message": "Authentication Error - invalid username", ...}
  SMS sending failed. Twilio responded with HTTP 401.
  ```
- Input validation for formats of Account SID and phone numbers is performed. If input is invalid, you will be re-prompted.
```
