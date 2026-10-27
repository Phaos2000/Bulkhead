// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include "Parameters.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <atomic>

namespace bulkhead {
namespace fs = std::filesystem;
inline bool validPresetName(const std::string& name) {
    if(name.empty() || name.size()>28 || name.front()==' ' || name.back()==' ')return false;
    for(unsigned char c:name)if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='-'))return false;
    if(name=="CON"||name=="PRN"||name=="AUX"||name=="NUL")return false;
    if(name.size()==4&&(name.substr(0,3)=="COM"||name.substr(0,3)=="LPT")&&name[3]>='1'&&name[3]<='9')return false;
    return true;
}
inline fs::path userPresetDirectory() {
    PWSTR path=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&path)))return {};
    fs::path result=fs::path(path)/L"Bulkhead2"/L"Presets";CoTaskMemFree(path);return result;
}
struct PresetEntry { std::string name; fs::path path; Values values; bool factory=false; };
class PresetLibrary {
public:
    explicit PresetLibrary(fs::path directory=userPresetDirectory()):folder(std::move(directory)){}
    std::vector<PresetEntry> scan() const {
        std::vector<PresetEntry> result;
        result.push_back({"INIT",{},defaults(),true});
        for(int i=0;i<presetCount;++i)result.push_back({presetNames[i],{},preset(i),true});
        std::vector<PresetEntry> user;
        std::error_code ec;
        if(!folder.empty())for(fs::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)) {
            const auto& path=it->path();
            if(path.extension()!=L".bulkhead" || !it->is_regular_file(ec))continue;
            const auto name=path.stem().string();Values v{};
            if(validPresetName(name)&&read(path,v))user.push_back({name,path,v,false});
        }
        std::sort(user.begin(),user.end(),[](const auto& a,const auto& b){return a.name<b.name;});
        result.insert(result.end(),user.begin(),user.end());return result;
    }
    static bool read(const fs::path& path,Values& values) {
        std::error_code ec;if(fs::file_size(path,ec)>16384 || ec)return false;
        std::ifstream in(path);in.imbue(std::locale::classic());
        std::string magic;int version=0,count=0;
        if(!(in>>magic>>version>>count)||magic!="BULKHEAD2"||version!=1||count!=Count)return false;
        Values parsed=defaults();std::array<bool,Count> seen{};
        for(int i=0;i<count;++i){int id=-1;double v=0;if(!(in>>id>>v)||id<0||id>=count||seen[id]||!std::isfinite(v)||v<0||v>1)return false;parsed[id]=canonical(id,v);seen[id]=true;}
        in>>std::ws;if(!in.eof())return false;
        parsed[Bypass]=0.;values=parsed;return true;
    }
    bool save(const std::string& name,const Values& values,bool overwrite,std::string& error) const {
        if(folder.empty()){error="PRESET FOLDER UNAVAILABLE";return false;}
        if(!validPresetName(name)){error="USE A-Z 0-9 SPACES OR DASH";return false;}
        for(double v:values)if(!std::isfinite(v)||v<0||v>1){error="INVALID PARAMETER VALUE";return false;}
        std::error_code ec;fs::create_directories(folder,ec);
        if(ec){error="CANNOT CREATE PRESET FOLDER";return false;}
        const auto target=folder/(name+".bulkhead");
        if(!overwrite && fs::exists(target,ec)){error="NAME EXISTS - CHOOSE ANOTHER";return false;}
        static std::atomic<unsigned> serial{0};
        auto temp=target;temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial);
        {
            std::ofstream out(temp,std::ios::trunc);out.imbue(std::locale::classic());
            out<<"BULKHEAD2 1 "<<Count<<"\n"<<std::setprecision(17);
            for(int i=0;i<Count;++i)out<<i<<" "<<(i==Bypass?0.:values[i])<<"\n";
            out.flush();if(!out){error="CANNOT WRITE PRESET";out.close();fs::remove(temp,ec);return false;}
        }
        if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH|(overwrite?MOVEFILE_REPLACE_EXISTING:0))){
            error="CANNOT SAVE - FILE MAY EXIST";fs::remove(temp,ec);return false;
        }
        return true;
    }
    bool rename(const PresetEntry& entry,const std::string& name,std::string& error) const {
        if(entry.factory||entry.path.parent_path()!=folder||!validPresetName(name)){error="INVALID USER PRESET NAME";return false;}
        if(entry.name==name)return true;
        if(!MoveFileW(entry.path.c_str(),(folder/(name+".bulkhead")).c_str())){error="CANNOT RENAME - NAME MAY EXIST";return false;}return true;
    }
    bool erase(const PresetEntry& entry,std::string& error) const {
        if(entry.factory||entry.path.parent_path()!=folder){error="FACTORY PRESETS ARE READ ONLY";return false;}
        std::error_code ec;if(!fs::remove(entry.path,ec)||ec){error="CANNOT DELETE PRESET";return false;}return true;
    }
private:fs::path folder;
};
}
