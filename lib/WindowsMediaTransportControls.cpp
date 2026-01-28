#ifdef _WIN32

#include "WindowsMediaTransportControls.h"
#include <spdlog/spdlog.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>

using namespace winrt;
using namespace Windows::Media;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Foundation;

WindowsMediaTransportControls::WindowsMediaTransportControls()
{
    try
    {
        // Check if SMTC is available on this system
        if (!IsAvailable())
        {
            spdlog::warn("Windows System Media Transport Controls not available on this system");
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception during WindowsMediaTransportControls construction: {}", e.what());
    }
}

WindowsMediaTransportControls::~WindowsMediaTransportControls()
{
    try
    {
        Shutdown();
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception during WindowsMediaTransportControls destruction: {}", e.what());
    }
}

bool WindowsMediaTransportControls::Initialize() noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_initialized)
        {
            return true;
        }

        // Get the system media transport controls
        m_smtc = SystemMediaTransportControls::GetForCurrentView();
        if (!m_smtc)
        {
            spdlog::error("Failed to get SystemMediaTransportControls");
            return false;
        }

        // Enable display updater so metadata is shown
        auto displayUpdater = m_smtc.DisplayUpdater();
        if (!displayUpdater)
        {
            spdlog::error("Failed to get DisplayUpdater");
            m_smtc = nullptr;
            return false;
        }

        // Set the music properties type
        displayUpdater.Type(MediaPlaybackType::Music);

        // Enable media control buttons
        m_smtc.IsEnabled(true);
        m_smtc.IsPlayEnabled(true);
        m_smtc.IsPauseEnabled(true);
        m_smtc.IsStopEnabled(true);
        m_smtc.IsNextEnabled(false);
        m_smtc.IsPreviousEnabled(false);

        m_initialized = true;
        spdlog::info("Windows System Media Transport Controls initialized successfully");

        return true;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception during SMTC initialization: {}", e.what());
        m_initialized = false;
        return false;
    }
}

void WindowsMediaTransportControls::Shutdown() noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_smtc)
        {
            m_smtc.IsEnabled(false);
            m_smtc = nullptr;
        }

        m_initialized = false;
        spdlog::info("Windows System Media Transport Controls shut down");
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception during SMTC shutdown: {}", e.what());
    }
}

void WindowsMediaTransportControls::SetPlaybackState(PlaybackState state) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || !m_smtc)
        {
            return;
        }

        m_smtc.PlaybackStatus(ToWinRTState(state));
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception setting playback state: {}", e.what());
    }
}

void WindowsMediaTransportControls::UpdateMetadata(const MediaMetadata& metadata) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || !m_smtc)
        {
            return;
        }

        auto displayUpdater = m_smtc.DisplayUpdater();
        if (!displayUpdater)
        {
            return;
        }

        auto musicProperties = displayUpdater.MusicProperties();
        if (!musicProperties)
        {
            return;
        }

        // Update music metadata
        musicProperties.Title(winrt::to_hstring(metadata.title.empty() ? "Unknown" : metadata.title));
        musicProperties.Artist(winrt::to_hstring(metadata.artist.empty() ? "Unknown Artist" : metadata.artist));
        musicProperties.AlbumTitle(winrt::to_hstring(metadata.album.empty() ? "Unknown Album" : metadata.album));

        // Update timeline properties with duration and current position
        auto timelineProperties = displayUpdater.TimelineProperties();
        if (timelineProperties && metadata.durationSeconds > 0)
        {
            Windows::Foundation::TimeSpan duration;
            duration.count = static_cast<int64_t>(metadata.durationSeconds) * 10000000LL; // Convert to 100-nanosecond units
            timelineProperties.MaxSeekTime(duration);

            Windows::Foundation::TimeSpan position;
            position.count = static_cast<int64_t>(metadata.positionSeconds) * 10000000LL;
            timelineProperties.Position(position);
        }

        // Update the display
        displayUpdater.Update();

        spdlog::debug("SMTC metadata updated - Title: {}, Artist: {}, Album: {}", 
                      metadata.title, metadata.artist, metadata.album);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception updating metadata: {}", e.what());
    }
}

void WindowsMediaTransportControls::SetAlbumArt(
    const unsigned char* imageData,
    size_t imageDataSize,
    const std::string& mimeType) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || !m_smtc || !imageData || imageDataSize == 0)
        {
            return;
        }

        auto displayUpdater = m_smtc.DisplayUpdater();
        if (!displayUpdater)
        {
            return;
        }

        // Create a thumbnail from the image data
        auto thumbnail = CreateThumbnail(imageData, imageDataSize, mimeType);
        if (!thumbnail)
        {
            spdlog::warn("Failed to create thumbnail for album art");
            return;
        }

        // Set the thumbnail
        displayUpdater.Thumbnail(thumbnail);
        displayUpdater.Update();

        spdlog::debug("SMTC album art updated - Size: {} bytes, Type: {}", imageDataSize, mimeType);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception setting album art: {}", e.what());
    }
}

void WindowsMediaTransportControls::ClearAlbumArt() noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || !m_smtc)
        {
            return;
        }

        auto displayUpdater = m_smtc.DisplayUpdater();
        if (!displayUpdater)
        {
            return;
        }

        displayUpdater.Thumbnail(nullptr);
        displayUpdater.Update();

        spdlog::debug("SMTC album art cleared");
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception clearing album art: {}", e.what());
    }
}

bool WindowsMediaTransportControls::IsInitialized() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

bool WindowsMediaTransportControls::IsAvailable() noexcept
{
    try
    {
        // Windows 10/11 with Media Control API
        // This will succeed if the API is available
        auto smtc = SystemMediaTransportControls::GetForCurrentView();
        return smtc != nullptr;
    }
    catch (...)
    {
        return false;
    }
}

IRandomAccessStreamReference WindowsMediaTransportControls::CreateThumbnail(
    const unsigned char* imageData,
    size_t imageDataSize,
    const std::string& mimeType) noexcept
{
    try
    {
        if (!imageData || imageDataSize == 0)
        {
            return nullptr;
        }

        // Create a memory stream from the image data
        auto stream = InMemoryRandomAccessStream();
        auto writer = DataWriter(stream);

        // Write the image data to the stream
        writer.WriteBytes(
            winrt::array_view<const uint8_t>(
                reinterpret_cast<const uint8_t*>(imageData),
                reinterpret_cast<const uint8_t*>(imageData) + imageDataSize
            )
        );

        auto op = writer.StoreAsync();
        op.get();

        // Seek to the beginning
        stream.Seek(0);

        // Create a RandomAccessStreamReference from the stream
        return RandomAccessStreamReference::CreateFromStream(stream);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception creating thumbnail: {}", e.what());
        return nullptr;
    }
}

MediaPlaybackState WindowsMediaTransportControls::ToWinRTState(PlaybackState state) noexcept
{
    switch (state)
    {
    case PlaybackState::Playing:
        return MediaPlaybackState::Playing;
    case PlaybackState::Paused:
        return MediaPlaybackState::Paused;
    case PlaybackState::Stopped:
        return MediaPlaybackState::Stopped;
    case PlaybackState::Opening:
        return MediaPlaybackState::Opening;
    case PlaybackState::Closed:
        return MediaPlaybackState::Closed;
    default:
        return MediaPlaybackState::Closed;
    }
}

#endif // _WIN32
