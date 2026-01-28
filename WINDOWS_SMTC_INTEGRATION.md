# Windows System Media Transport Controls Integration

## Overview

This document describes the Windows System Media Transport Controls (SMTC) integration added to ShairportQt. This feature allows ShairportQt to publish now-playing metadata to the Windows UI, making it accessible through:

- **Lock Screen**: Display song information on the Windows lock screen
- **Windows Taskbar**: Show playback controls and metadata in the taskbar
- **Media Control Panel**: Integrate with Windows' native media control system
- **Third-party Applications**: Allow other applications to access now-playing information via Windows Media Control API

## Features

### Metadata Publishing
The following metadata is published to Windows:
- **Track Title**: The name of the currently playing song
- **Artist Name**: The artist of the currently playing song
- **Album Name**: The album title
- **Album Art**: Album artwork thumbnail
- **Duration & Position**: Track duration and current playback position

### Playback State
The integration tracks and publishes the following playback states:
- **Playing**: Audio is currently playing
- **Paused**: Playback is paused
- **Stopped**: Playback is stopped
- **Opening**: Media is being loaded
- **Closed**: No media is active

## Architecture

### Key Components

#### 1. **WindowsMediaTransportControls Class** (`inc/WindowsMediaTransportControls.h`, `lib/WindowsMediaTransportControls.cpp`)

The main class that handles all SMTC operations:

**Key Methods:**
- `Initialize()`: Initializes the SMTC connection
- `Shutdown()`: Cleanly shuts down SMTC
- `UpdateMetadata()`: Updates track metadata (title, artist, album, duration, position)
- `SetPlaybackState()`: Updates the current playback state
- `SetAlbumArt()`: Sets the album artwork thumbnail
- `ClearAlbumArt()`: Removes the album artwork
- `IsInitialized()`: Checks if SMTC is ready
- `IsAvailable()`: Static method to check if SMTC is available on the system

**Platform Support:**
- Windows 10 and later (using Windows Runtime/WinRT)
- Automatically compiled only on Windows builds
- Gracefully disabled on non-Windows platforms

#### 2. **Integration with MainDlg**

The integration point in the main application dialog:

**Initialization:** 
- SMTC is initialized in `MainDlg::MainDlg()` constructor
- Availability is checked before initialization
- Graceful fallback if SMTC is unavailable

**Metadata Updates:**
- `OnDmapInfo()`: Triggered when track metadata changes
  - Updates title, artist, album, and duration/position
- `OnPlayState()`: Triggered when playback state changes
  - Updates the playback state (Playing/Paused/Stopped)
- `OnAlbumArt()`: Triggered when album art is received
  - Sets the album artwork thumbnail
  - Clears artwork when "NONE" is received

## Usage

### User Perspective

Once enabled, users will see:

1. **Lock Screen Integration**:
   - Song title, artist, and album appear on the lock screen when music is playing
   - Album artwork is displayed as a thumbnail

2. **Taskbar Integration**:
   - Music information appears in the taskbar's media player widget
   - Quick access to play/pause controls (if configured)

3. **Media Control Settings**:
   - ShairportQt appears in Windows Settings → Apps → Advanced app settings → App execution aliases
   - Users can adjust app permissions related to media controls

### Developer Perspective

The integration is seamless and requires no configuration:

1. **Automatic Initialization**: SMTC is automatically initialized when the application starts on Windows
2. **Automatic Metadata Sync**: Metadata is automatically synchronized whenever it changes
3. **Thread-Safe**: All operations are protected with mutexes for thread safety
4. **Error Handling**: Graceful fallback if SMTC is unavailable

## Technical Details

### WinRT/Windows Runtime

The implementation uses Windows Runtime (WinRT) C++ bindings:

```cpp
// Core SMTC API
winrt::Windows::Media::SystemMediaTransportControls m_smtc;

// Display properties
auto displayUpdater = m_smtc.DisplayUpdater();
displayUpdater.Type(MediaPlaybackType::Music);
displayUpdater.MusicProperties(); // Title, Artist, Album
displayUpdater.TimelineProperties(); // Duration, Position
displayUpdater.Thumbnail(); // Album Art
```

### Thread Safety

The implementation uses a `std::mutex` to protect shared state:
- All public methods acquire the lock before accessing SMTC
- Metadata updates are serialized to prevent race conditions
- Album art thumbnail creation is thread-safe

### Memory Management

- Uses smart pointers (`std::shared_ptr`) for SMTC instance management
- Album art is stored as raw binary data (platform-independent)
- Automatic cleanup in destructor

### Error Handling

All methods have `noexcept` specifications:
- Exceptions are caught and logged
- Failures don't crash the application
- Graceful degradation if SMTC is unavailable

## Compilation

### Windows Build

The integration is automatically compiled on Windows builds:

```cmake
# CMakeLists.txt
if (WIN32)
    list(APPEND LIBRARY_SOURCES lib/WindowsMediaTransportControls.cpp)
endif()

# Link Windows Runtime libraries
if (WIN32)
    target_link_libraries(ShairportQt PRIVATE windowsapp)
endif()
```

**Requirements:**
- Windows 10 SDK or later
- Visual Studio 2019+ (for WinRT C++ support)
- `/std:c++17` or later

### Non-Windows Build

The code is wrapped in `#ifdef _WIN32` guards:
- Completely excluded from non-Windows builds
- No compilation errors on Linux, macOS, etc.
- Zero overhead on non-Windows platforms

## Debugging

### Logging

The integration uses spdlog for logging:

```
INFO: Windows System Media Transport Controls initialized successfully
DEBUG: SMTC metadata updated - Title: Song, Artist: Artist Name, Album: Album Name
DEBUG: SMTC album art updated - Size: 12345 bytes, Type: image/jpeg
```

### Testing

To verify the integration is working:

1. **On Windows 10/11**: 
   - Open Windows Settings → Notifications & Actions
   - Scroll to "Media player" and verify ShairportQt appears
   
2. **Lock Screen**:
   - Start playing audio
   - Press Win+L to lock the screen
   - Verify song information and artwork appear

3. **Taskbar**:
   - Check the system tray area
   - Right-click the taskbar and look for media controls

## Example Flow

```
User receives AirPlay stream
    ↓
Metadata arrives (title, artist, album)
    ↓
OnSetCurrentDmapInfo() called
    ↓
OnDmapInfo() Qt slot invoked
    ↓
UpdateMetadata() called on SMTC instance
    ↓
Windows updates lock screen & taskbar
    ↓
User sees song info on lock screen/taskbar
```

## Limitations & Future Enhancements

### Current Limitations
- Read-only metadata display (Windows doesn't allow metadata modification from SMTC)
- Album art limited to thumbnail size for performance
- No support for playback rate changes via SMTC buttons

### Possible Future Enhancements
1. **Media Control Buttons**: Support for next/previous/seek controls via SMTC
2. **Album Art Caching**: Cache album artwork for performance
3. **Settings**: Allow users to disable SMTC integration
4. **Multiple Transports**: Support for multiple simultaneous SMTC connections
5. **Ratings & Feedback**: Integration with Windows' media rating system

## References

- [Windows Media Control API Documentation](https://docs.microsoft.com/en-us/windows/win32/controls/about-custom-draw)
- [SystemMediaTransportControls Class](https://docs.microsoft.com/en-us/uwp/api/windows.media.systemediatransportcontrols)
- [WinRT C++ Documentation](https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/)
- [SMTC DisplayUpdater](https://docs.microsoft.com/en-us/uwp/api/windows.media.playback.mediatransportcontrolsdisplayupdater)

## License

This integration follows the same license as the ShairportQt project.
