#ifndef TELEMETRY_SERVER_H
#define TELEMETRY_SERVER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace ara
{
    namespace telemetry
    {
        class TelemetryHub;

        /// @brief Local HTTP server which serves the cockpit dashboard and streams
        /// the runtime telemetry snapshots of a hub over server-sent events
        /// @note The class is NOT part of the Adaptive AUTOSAR standard.
        class TelemetryServer final
        {
        public:
            /// @brief Interval in milliseconds between two streamed snapshots
            static const int64_t cStreamIntervalMs{200};

            /// @brief Constructor
            /// @param hub Telemetry hub to be served
            /// @param port TCP port to listen on
            /// @param webRoot Directory which contains the dashboard static files
            TelemetryServer(TelemetryHub *hub, uint16_t port, std::string webRoot);

            ~TelemetryServer() noexcept;

            TelemetryServer(const TelemetryServer &) = delete;
            TelemetryServer &operator=(const TelemetryServer &) = delete;

            /// @brief Start listening on the loopback interface
            /// @returns True if the server has been started successfully; otherwise false
            bool Start();

            /// @brief Stop the server and wait for its connection handlers
            void Stop() noexcept;

            /// @brief Get the port which the server is listening on
            /// @returns TCP port number
            uint16_t Port() const noexcept;

        private:
            void accepting();
            void handleConnection(int clientDescriptor);
            void streamTelemetry(int clientDescriptor);
            void serveFile(int clientDescriptor, const std::string &path);
            void respond(
                int clientDescriptor,
                const std::string &status,
                const std::string &contentType,
                const std::string &body);
            bool sendAll(int clientDescriptor, const std::string &payload);
            void joinConnections();

            TelemetryHub *const mHub;
            const std::string mWebRoot;
            uint16_t mPort;
            int mListenDescriptor;
            std::atomic_bool mRunning;
            std::thread mAcceptingThread;
            std::vector<std::thread> mConnectionThreads;
        };
    }
}

#endif
