// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace bulkhead {
inline std::filesystem::path settingsDirectory() {
    wchar_t overridePath[32768]{};
    const auto length=GetEnvironmentVariableW(L"BULKHEAD2_SETTINGS_DIR",overridePath,32768);
    if(length>0&&length<32768)return std::filesystem::path(overridePath);
    PWSTR path=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&path)))return {};
    const auto result=std::filesystem::path(path)/L"Bulkhead2";CoTaskMemFree(path);return result;
}
inline int loadPreferredZoom() noexcept {
    try{const auto folder=settingsDirectory();if(folder.empty())return 3;int zoom=0;std::ifstream in(folder/L"ui-size.txt");if(in>>zoom&&zoom>=1&&zoom<=5)return zoom;}catch(...){}return 3;
}
inline bool savePreferredZoom(int zoom) noexcept {
    try{const auto folder=settingsDirectory();if(folder.empty())return false;std::error_code error;std::filesystem::create_directories(folder,error);if(error)return false;
        const auto target=folder/L"ui-size.txt",temporary=folder/(L"ui-size-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
        {std::ofstream out(temporary,std::ios::trunc);out<<std::clamp(zoom,1,5)<<"\n";if(!out)return false;}
        return MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    }catch(...){return false;}
}
inline bool loadPreferredCursor() noexcept {
    try{const auto folder=settingsDirectory();int enabled=1;std::ifstream in(folder/L"custom-cursor.txt");if(in>>enabled&&(enabled==0||enabled==1))return enabled!=0;}catch(...){}return true;
}
inline bool savePreferredCursor(bool enabled) noexcept {
    try{const auto folder=settingsDirectory();if(folder.empty())return false;std::error_code error;std::filesystem::create_directories(folder,error);if(error)return false;
        const auto target=folder/L"custom-cursor.txt",temporary=folder/(L"cursor-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
        {std::ofstream out(temporary,std::ios::trunc);out<<(enabled?1:0)<<"\n";if(!out)return false;}
        return MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    }catch(...){return false;}
}

inline int loadPreferredCursorStyle() noexcept {
    try{const auto folder=settingsDirectory();int style=5;std::ifstream in(folder/L"cursor-palette.txt");if(in>>style&&style>=0&&style<=5)return style==1?5:style;}catch(...){}return 5;
}
inline bool savePreferredCursorStyle(int style) noexcept {
    try{const auto folder=settingsDirectory();if(folder.empty())return false;std::error_code error;std::filesystem::create_directories(folder,error);if(error)return false;
        const auto target=folder/L"cursor-palette.txt",temporary=folder/(L"cursor-palette-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
        {std::ofstream out(temporary,std::ios::trunc);out<<std::clamp(style,0,5)<<"\n";if(!out)return false;}
        return MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    }catch(...){return false;}
}

}
