# Remote PC Control via Email and LAN

This project allows remote control of a server PC via email and LAN using C++. The administrator sends control commands through email, and the client processes the commands, communicates with the server over LAN, and sends back responses. The system uses external libraries, `cURL` and `Chilkat`, for email reading and sending.

## Features
- **Shutdown**: Turn off the server.
- **Reset**: Restart the server.
- **Keylogger**: Capture keystrokes from the server.
- **App Control**: Start or stop applications.
- **List Applications**: Show a list of running applications.
- **Process Management**: Manage running processes on the server.
- **File Management**: Copy or delete files.
- **Webcam Control**: Open the webcam for live video.
- **Screenshot**: Capture the server screen.

## Architecture
1. **Admin**: Sends control commands via email from `admin@gmail.com`.
2. **Client**:
   - Checks emails for commands sent by Admin.
   - Processes the command and sends it to the server over LAN.
   - Sends results back to Admin via email.
3. **Server**:
   - Receives commands from the client via LAN.
   - Executes the requested operations.
   - Returns results to the client.

## Prerequisites
### Libraries
- **cURL**: For LAN communication.
- **Chilkat**: For email reading and sending.

### Development Environment
- **C++ Compiler**: Compatible with the libraries.
- **Network Setup**: Ensure LAN connection between Client and Server.

## Setup Instructions

1. **Install Libraries**:
   - Download and install `cURL`.
   - Download and integrate `Chilkat` with your C++ project.

2. **Email Configuration**:
   - Use a Gmail account for both Admin (`admin@gmail.com`) and Client (`client@gmail.com`).
   - Enable IMAP and SMTP in Gmail settings.
   - Allow less secure app access or generate app-specific passwords if using two-factor authentication.

3. **Network Configuration**:
   - Ensure Client and Server are connected via the same LAN.
   - Assign static IP addresses for stable communication.

4. **Compile and Run**:
   - Compile the project with the necessary library includes.
   - Run the Client on the client machine and Server on the server machine.

## Command List
The Admin sends commands via email in the following format:

### Example Email Command
**Subject**: `Control Request`
**Body**:
```
COMMAND: <command_name>
PARAMS: <parameters>
```

### Available Commands
| Command      | Parameters          | Description                       |
|--------------|---------------------|-----------------------------------|
| SHUTDOWN     | None                | Shutdown the server.             |
| RESET        | None                | Restart the server.              |
| KEYLOGGER_START    | None                | Start the keylogger.             |
| KEYLOGGER_STOP    | None                | Stop the keylogger.             |
| START_APP    | <app_name>          | Start an application.            |
| STOP_APP     | <app_name>          | Stop an application.             |
| LIST_APPS    | None                | Show a list of running apps.     |
| LIST_PROCESSES | None              | Show running processes.          |
| COPY_FILE    | <src> <dest>        | Copy a file.                     |
| DELETE_FILE  | <file_path>         | Delete a file.                   |
| OPEN_WEBCAM  | None                | Open the webcam.                 |
| SCREENSHOT   | None                | Take a screenshot.               |

## Workflow
1. Admin sends a control command email to `client@gmail.com`.
2. The Client reads the email using Chilkat.
3. The Client parses the command and sends it to the Server over LAN using cURL.
4. The Server processes the command and returns the result to the Client.
5. The Client emails the result back to the Admin.

## Example Usage
- **Shutdown**:
  **Email Body**:
  ```
  COMMAND: SHUTDOWN
  ```
- **Start Application**:
  **Email Body**:
  ```
  COMMAND: START_APP
  PARAMS: notepad.exe
  ```

## Notes
- Ensure proper email authentication settings to avoid connection issues.
- Ensure network stability for uninterrupted LAN communication.
- Use with caution and comply with local laws regarding keylogging and remote control.

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Acknowledgments
- [cURL](https://curl.se/) for HTTP communication.
- [Chilkat](https://www.chilkatsoft.com/) for email handling.
