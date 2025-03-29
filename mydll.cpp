#include <windows.h>
#include <iostream>

#define PIPE_NAME "\\\\.\\pipe\\BossPipe"

DWORD WINAPI PipeListener(LPVOID)
{
    HANDLE hPipe;
    char buffer[128];
    DWORD bytesRead;

    hPipe = CreateNamedPipeA(
        PIPE_NAME, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 128, 128, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        std::cout << "[DLL] Failed to create named pipe.\n";
        return 1;
    }

    std::cout << "[DLL] Waiting for pipe connection...\n";

    if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED)
    {
        std::cout << "[DLL] Pipe connected. Listening for boss updates...\n";

        while (true)
        {
            if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
            {
                buffer[bytesRead] = '\0'; // Null-terminate the received data
                int bossCode = atoi(buffer);
                std::cout << "[DLL] Received Boss Code: " << bossCode << "\n";
            }
            else
            {
                std::cout << "[DLL] Pipe read error.\n";
                break;
            }
        }
    }

    CloseHandle(hPipe);
    std::cout << "[DLL] Pipe closed.\n";
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule); // Prevent thread attach/detach calls
        CreateThread(NULL, 0, PipeListener, NULL, 0, NULL); // Use CreateThread instead of std::thread
    }
    return TRUE;
}
