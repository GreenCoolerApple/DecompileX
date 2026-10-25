#include "ISOCompiler.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <vector>

namespace DecompileX {
namespace {
namespace fs = std::filesystem;
constexpr std::uint32_t Sector = 2048;
struct Node {
    std::string name;
    fs::path source;
    bool directory = true;
    Node* parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
    std::uint32_t extent = 0;
    std::uint32_t size = 0;
    std::uint16_t pathIndex = 0;
};
void le16(std::uint8_t* p, std::uint16_t v) { p[0] = static_cast<std::uint8_t>(v); p[1] = static_cast<std::uint8_t>(v >> 8); }
void be16(std::uint8_t* p, std::uint16_t v) { p[0] = static_cast<std::uint8_t>(v >> 8); p[1] = static_cast<std::uint8_t>(v); }
void le32(std::uint8_t* p, std::uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i)); }
void be32(std::uint8_t* p, std::uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * (3 - i))); }
void both16(std::uint8_t* p, std::uint16_t v) { le16(p,v); be16(p+2,v); }
void both32(std::uint8_t* p, std::uint32_t v) { le32(p,v); be32(p+4,v); }
std::uint32_t blocks(std::uint64_t n) { return static_cast<std::uint32_t>((n + Sector - 1) / Sector); }
std::string identifier(std::string name, bool dir) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    std::string clean;
    for (unsigned char c : name) clean += (std::isalnum(c) && c < 128) || c == '_' || c == '.' ? static_cast<char>(c) : '_';
    if (dir) { if (clean.size() > 8) clean.resize(8); return clean.empty() ? "DIR" : clean; }
    auto dot = clean.find_last_of('.');
    std::string stem = dot == std::string::npos ? clean : clean.substr(0,dot);
    std::string ext = dot == std::string::npos ? "" : clean.substr(dot+1);
    stem.resize(std::min<std::size_t>(stem.size(),8)); ext.resize(std::min<std::size_t>(ext.size(),3));
    if (stem.empty()) stem = "FILE";
    return stem + (ext.empty() ? "" : "." + ext) + ";1";
}
std::size_t recordSize(std::size_t n) { return 33 + n + (n % 2 == 0 ? 1 : 0); }
std::vector<std::uint8_t> record(const Node& n, std::uint8_t special=255) {
    std::string id = special == 255 ? n.name : std::string(1,static_cast<char>(special));
    std::vector<std::uint8_t> r(recordSize(id.size()),0);
    r[0] = static_cast<std::uint8_t>(r.size());
    both32(r.data()+2,n.extent); both32(r.data()+10,n.size);
    r[18]=126; r[19]=1; r[20]=1; r[21]=0; r[22]=0; r[23]=0; r[24]=0;
    r[25] = n.directory ? 2 : 0; both16(r.data()+28,1);
    r[32]=static_cast<std::uint8_t>(id.size());
    std::copy(id.begin(),id.end(),r.begin()+33);
    return r;
}
std::uint32_t directoryBytes(const Node& dir) {
    std::uint64_t pos=0;
    auto add=[&](std::size_t len) { if (pos % Sector + len > Sector) pos += Sector - pos % Sector; pos += len; };
    add(recordSize(1)); add(recordSize(1));
    for (const auto& child:dir.children) add(recordSize(child->name.size()));
    return blocks(pos)*Sector;
}
void pad(std::ofstream& out, std::uint64_t count) {
    std::array<char,Sector> zeros{};
    while (count) { auto n=static_cast<std::streamsize>(std::min<std::uint64_t>(count,zeros.size())); out.write(zeros.data(),n); count-=static_cast<std::uint64_t>(n); }
}
void writeDirectory(std::ofstream& out,const Node& dir) {
    std::vector<std::uint8_t> buffer(dir.size,0);
    std::size_t pos=0;
    auto append=[&](const std::vector<std::uint8_t>& r) {
        if (pos % Sector + r.size() > Sector) pos += Sector - pos % Sector;
        std::copy(r.begin(),r.end(),buffer.begin()+static_cast<std::ptrdiff_t>(pos)); pos+=r.size();
    };
    append(record(dir,0)); append(record(dir.parent ? *dir.parent : dir,1));
    for (const auto& child:dir.children) append(record(*child));
    out.write(reinterpret_cast<const char*>(buffer.data()),static_cast<std::streamsize>(buffer.size()));
}
void writePathTable(std::ofstream& out,const std::vector<Node*>& dirs,bool big,std::uint32_t sectors) {
    std::vector<std::uint8_t> data;
    for (const Node* dir:dirs) {
        std::string id=dir->parent ? dir->name : std::string(1,'\0');
        std::size_t old=data.size(); data.resize(old+8+id.size()+(id.size()%2),0);
        data[old]=static_cast<std::uint8_t>(id.size());
        if (big) { be32(data.data()+old+2,dir->extent); be16(data.data()+old+6,dir->parent ? dir->parent->pathIndex : 1); }
        else { le32(data.data()+old+2,dir->extent); le16(data.data()+old+6,dir->parent ? dir->parent->pathIndex : 1); }
        std::copy(id.begin(),id.end(),data.begin()+static_cast<std::ptrdiff_t>(old+8));
    }
    out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
    pad(out,static_cast<std::uint64_t>(sectors)*Sector-data.size());
}
}
ISOResult ISOCompiler::compile(const fs::path& sourceDirectory,const fs::path& outputISO,const std::string& volumeLabel) {
    ISOResult result;
    std::error_code ec;
    if (!fs::is_directory(sourceDirectory,ec) || ec) { result.message="Source directory does not exist."; return result; }
    fs::path source=fs::weakly_canonical(sourceDirectory,ec);
    if (ec) { result.message="Cannot resolve source directory."; return result; }
    fs::path output=fs::absolute(outputISO,ec);
    if (ec || outputISO.empty()) { result.message="Invalid output path."; return result; }
    fs::path parent=output.parent_path();
    if (!fs::exists(parent,ec) || ec) { result.message="Output directory does not exist."; return result; }
    output=fs::weakly_canonical(parent,ec)/output.filename();
    if (ec) { result.message="Cannot resolve output path."; return result; }
    auto isInside=[&](const fs::path& candidate) { auto a=source.begin(),b=candidate.begin(); for (;a!=source.end() && b!=candidate.end() && *a==*b;++a,++b) {} return a==source.end(); };
    if (isInside(output)) { result.message="Output ISO must be outside the source directory."; return result; }
    Node root; root.name=std::string(1,'\0'); root.source=source;
    std::vector<Node*> directories{&root}; std::vector<Node*> files;
    std::size_t count=0;
    auto scan=[&](auto&& self,Node& dir,std::size_t depth)->bool {
        if (depth>64) { result.message="Directory nesting exceeds limit."; return false; }
        std::set<std::string> names;
        fs::directory_iterator it(dir.source,fs::directory_options::none,ec),end;
        if (ec) { result.message="Cannot list directory."; return false; }
        for (;it!=end;it.increment(ec)) {
            if (ec) { result.message="Directory traversal failed."; return false; }
            if (it->is_symlink(ec) || ec) { result.message="Symbolic links are not supported."; return false; }
            bool isDir=it->is_directory(ec);
            if (ec) { result.message="Cannot inspect entry."; return false; }
            if (!isDir && !it->is_regular_file(ec)) { result.message="Unsupported filesystem entry."; return false; }
            if (ec) { result.message="Cannot inspect entry."; return false; }
            auto child=std::make_unique<Node>(); child->directory=isDir; child->parent=&dir; child->source=it->path();
            child->name=identifier(it->path().filename().string(),isDir);
            if (!names.insert(child->name).second) { result.message="ISO 9660 filename collision: "+child->name; return false; }
            if (!isDir) {
                auto size=fs::file_size(child->source,ec);
                if (ec || size>std::numeric_limits<std::uint32_t>::max()) { result.message="File exceeds ISO 9660 size limit."; return false; }
                child->size=static_cast<std::uint32_t>(size);
            }
            dir.children.push_back(std::move(child));
            if (++count>100000) { result.message="Too many entries."; return false; }
        }
        if (ec) { result.message="Directory traversal failed."; return false; }
        std::sort(dir.children.begin(),dir.children.end(),[](const auto& a,const auto& b){return a->name<b->name;});
        for (auto& child:dir.children) if (child->directory && !self(self,*child,depth+1)) return false;
        return true;
    };
    if (!scan(scan,root,0)) return result;
    for (std::size_t i=0;i<directories.size();++i) {
        for (auto& child:directories[i]->children) if (child->directory) {
            if (directories.size()>=65535) { result.message="Too many directories."; return result; }
            child->pathIndex=static_cast<std::uint16_t>(directories.size()+1);
            directories.push_back(child.get());
        }
    }
    root.pathIndex=1;
    std::uint64_t pathBytes=0;
    for (const auto* dir:directories) { auto n=dir->parent ? dir->name.size() : 1; pathBytes+=8+n+(n%2); }
    if (pathBytes>std::numeric_limits<std::uint32_t>::max()) { result.message="Path table too large."; return result; }
    std::uint32_t pathSectors=blocks(pathBytes);
    std::uint64_t cursor=18ull+2ull*pathSectors;
    for (Node* dir:directories) {
        dir->size=directoryBytes(*dir);
        dir->extent=static_cast<std::uint32_t>(cursor);
        cursor+=blocks(dir->size);
    }
    for (Node* dir:directories) for (auto& child:dir->children) if (!child->directory) files.push_back(child.get());
    for (Node* file:files) { file->extent=static_cast<std::uint32_t>(cursor); cursor+=blocks(file->size); }
    if (cursor>std::numeric_limits<std::uint32_t>::max() || cursor*Sector>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        result.message="ISO image exceeds supported size."; return result;
    }
    std::ofstream out(output,std::ios::binary|std::ios::trunc);
    if (!out) { result.message="Cannot create output ISO."; return result; }
    pad(out,16ull*Sector);
    std::array<std::uint8_t,Sector> pvd{};
    pvd[0]=1; std::memcpy(pvd.data()+1,"CD001",5); pvd[6]=1;
    std::fill(pvd.begin()+8,pvd.begin()+40,' ');
    std::fill(pvd.begin()+40,pvd.begin()+72,' ');
    std::string label=volumeLabel.empty()?"DECOMPILEX":volumeLabel;
    std::transform(label.begin(),label.end(),label.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    label.resize(std::min<std::size_t>(label.size(),32));
    for (char& c:label) if (!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c==' ')) c='_';
    std::copy(label.begin(),label.end(),pvd.begin()+40);
    both32(pvd.data()+80,static_cast<std::uint32_t>(cursor));
    both16(pvd.data()+120,1); both16(pvd.data()+124,1); both16(pvd.data()+128,Sector);
    both32(pvd.data()+132,static_cast<std::uint32_t>(pathBytes));
    le32(pvd.data()+140,18); be32(pvd.data()+148,18+pathSectors);
    auto rootRecord=record(root,0);
    std::copy(rootRecord.begin(),rootRecord.end(),pvd.begin()+156);
    pvd[881]=1;
    out.write(reinterpret_cast<const char*>(pvd.data()),Sector);
    std::array<std::uint8_t,Sector> term{}; term[0]=255; std::memcpy(term.data()+1,"CD001",5); term[6]=1;
    out.write(reinterpret_cast<const char*>(term.data()),Sector);
    writePathTable(out,directories,false,pathSectors);
    writePathTable(out,directories,true,pathSectors);
    for (const Node* dir:directories) writeDirectory(out,*dir);
    std::array<char,64*1024> buffer{};
    for (const Node* file:files) {
        std::ifstream in(file->source,std::ios::binary);
        if (!in) { result.message="Cannot read file: "+file->source.string(); out.close(); fs::remove(output,ec); return result; }
        std::uint64_t remaining=file->size;
        while (remaining) {
            auto n=static_cast<std::streamsize>(std::min<std::uint64_t>(remaining,buffer.size()));
            in.read(buffer.data(),n);
            if (in.gcount()!=n) { result.message="File read failed."; out.close(); fs::remove(output,ec); return result; }
            out.write(buffer.data(),n);
            remaining-=static_cast<std::uint64_t>(n);
        }
        pad(out,static_cast<std::uint64_t>(blocks(file->size))*Sector-file->size);
        ++result.filesAdded;
    }
    out.flush(); bool ok=out.good(); out.close();
    if (!ok) { result.message="Failed to write ISO."; fs::remove(output,ec); return result; }
    result.success=true; result.message="ISO image created successfully."; result.bytesWritten=cursor*Sector;
    return result;
}
}
