#include "IsoCodeScanner.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
namespace DecompileX {
namespace {
constexpr std::uint64_t sector = 2048;
std::uint32_t le32(const unsigned char* p) { return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24); }
std::uint16_t le16(const unsigned char* p) { return std::uint16_t(p[0]) | (std::uint16_t(p[1])<<8); }
std::string trim(std::string s) { while (!s.empty() && s.back()==' ') s.pop_back(); return s; }
std::string upper(std::string s) { for (char& c:s) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return s; }
bool ends(const std::string& s,const std::string& suffix) { return s.size()>=suffix.size() && s.compare(s.size()-suffix.size(),suffix.size(),suffix)==0; }
class Reader {
public:
    std::ifstream in;
    std::uint64_t length=0;
    explicit Reader(const std::filesystem::path& p) {
        in.open(p,std::ios::binary);
        if (!in) throw std::runtime_error("Cannot open ISO file");
        in.seekg(0,std::ios::end);
        auto n=in.tellg();
        if (n<0) throw std::runtime_error("Cannot determine ISO size");
        length=static_cast<std::uint64_t>(n);
    }
    bool read(std::uint64_t offset,unsigned char* data,std::size_t size) {
        if (offset>length || size>length-offset || offset>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) return false;
        in.clear(); in.seekg(static_cast<std::streamoff>(offset));
        return static_cast<bool>(in.read(reinterpret_cast<char*>(data),static_cast<std::streamsize>(size)));
    }
};
std::string identify(const std::array<unsigned char,64>& b,std::size_t count,const std::string& path) {
    if (count>=4 && b[0]==0x7f && b[1]=='E' && b[2]=='L' && b[3]=='F') {
        if (count>=20 && b[4]==1 && le16(b.data()+18)==8) return "ELF32 MIPS executable/module";
        return "ELF executable/module";
    }
    if (count>=4 && b[0]==0 && b[1]=='P' && b[2]=='B' && b[3]=='P') return "PSP PBP container";
    if (count>=4 && b[0]=='~' && b[1]=='P' && b[2]=='S' && b[3]=='P') return "PSP encrypted/packed module";
    if (count>=4 && b[0]=='P' && b[1]=='K' && b[2]==3 && b[3]==4) return "ZIP archive";
    if (count>=2 && b[0]=='M' && b[1]=='Z') return "PE executable";
    if (count>=4 && b[0]==0x1b && b[1]=='L' && b[2]=='u' && b[3]=='a') return "Lua bytecode";
    if (count>=4 && b[0]==0x03 && b[1]==0xF3 && b[2]==0x0D && b[3]==0x0A) return "Python bytecode (signature candidate)";
    auto name=upper(path);
    if (ends(name,".PRX") || ends(name,"EBOOT.BIN") || ends(name,"BOOT.BIN")) return "PSP executable candidate (unrecognized header)";
    if (ends(name,".ELF") || ends(name,".SELF")) return "Executable candidate (unrecognized header)";
    if (ends(name,".LUA") || ends(name,".JS") || ends(name,".PY") || ends(name,".CS") || ends(name,".XML")) return "Script/source candidate";
    return {};
}
struct Dir { std::uint32_t lba; std::uint32_t size; std::string path; std::size_t depth; };
}
std::string IsoCodeReport::formatText() const {
    std::ostringstream s;
    s << "DecompileX ISO Code Scanner v0.0.1\nStatus: " << (success?"OK":"ERROR") << "\n";
    if (!message.empty()) s << "Message: " << message << "\n";
    s << "Volume: " << volume << "\nImage bytes: " << imageBytes << "\nFiles scanned: " << filesScanned << "\nCandidates: " << matches.size() << "\n\n";
    for (const auto& e:matches) s << e.path << "\n  Type: " << e.kind << "\n  Size: " << e.size << " bytes\n  ISO offset: " << e.offset << "\n\n";
    s << "Signatures and filenames identify candidates, not recovered source code.\n";
    return s.str();
}
IsoCodeReport IsoCodeScanner::scan(const std::filesystem::path& iso,std::size_t maxEntries) {
    IsoCodeReport r;
    try {
        Reader f(iso); r.imageBytes=f.length;
        if (f.length < 18*sector) throw std::runtime_error("ISO is too small");
        std::array<unsigned char,2048> descriptor{};
        bool found=false;
        for (std::uint64_t n=16;n<80 && (n+1)*sector<=f.length;++n) {
            if (!f.read(n*sector,descriptor.data(),descriptor.size())) throw std::runtime_error("Cannot read volume descriptor");
            if (std::string(reinterpret_cast<const char*>(descriptor.data()+1),5)!="CD001" || descriptor[6]!=1) throw std::runtime_error("Invalid ISO 9660 descriptor");
            if (descriptor[0]==1) { found=true; break; }
            if (descriptor[0]==255) break;
        }
        if (!found) throw std::runtime_error("ISO 9660 primary volume descriptor missing (UDF-only images unsupported)");
        r.volume=trim(std::string(reinterpret_cast<const char*>(descriptor.data()+40),32));
        const auto* root=descriptor.data()+156;
        if (root[0]<34 || (root[25]&2)==0) throw std::runtime_error("Invalid root directory");
        std::vector<Dir> todo{{le32(root+2),le32(root+10),"",0}};
        std::unordered_set<std::uint64_t> seen;
        std::size_t total=0;
        while (!todo.empty()) {
            Dir d=std::move(todo.back()); todo.pop_back();
            if (d.depth>64) throw std::runtime_error("ISO directory depth limit exceeded");
            auto key=(std::uint64_t(d.lba)<<32)|d.size;
            if (!seen.insert(key).second) continue;
            std::uint64_t start=std::uint64_t(d.lba)*sector;
            if (start>f.length || d.size>f.length-start) throw std::runtime_error("Directory extent outside ISO");
            for (std::uint64_t pos=0;pos<d.size;) {
                unsigned char size=0;
                if (!f.read(start+pos,&size,1)) throw std::runtime_error("Cannot read directory record");
                if (!size) { pos=((pos/sector)+1)*sector; continue; }
                if (size<34 || size>sector-(pos%sector) || size>d.size-pos) throw std::runtime_error("Malformed ISO directory record");
                std::array<unsigned char,255> rec{};
                if (!f.read(start+pos,rec.data(),size)) throw std::runtime_error("Cannot read ISO entry");
                pos+=size;
                std::size_t nameLen=rec[32];
                if (33+nameLen>size) throw std::runtime_error("Malformed ISO filename");
                if (nameLen==1 && (rec[33]==0 || rec[33]==1)) continue;
                std::string name(reinterpret_cast<const char*>(rec.data()+33),nameLen);
                auto semi=name.find(';'); if (semi!=std::string::npos) name.erase(semi);
                if (name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos || name.find('\\')!=std::string::npos) continue;
                if (++total>maxEntries) throw std::runtime_error("ISO entry limit exceeded");
                std::string path=d.path.empty()?name:d.path+"/"+name;
                std::uint32_t lba=le32(rec.data()+2), bytes=le32(rec.data()+10);
                std::uint64_t offset=std::uint64_t(lba)*sector;
                if (offset>f.length || bytes>f.length-offset) throw std::runtime_error("File extent outside ISO");
                if (rec[25]&2) { todo.push_back({lba,bytes,path,d.depth+1}); continue; }
                ++r.filesScanned;
                std::array<unsigned char,64> sample{};
                auto sampleLen=static_cast<std::size_t>(std::min<std::uint32_t>(bytes,64));
                if (sampleLen && !f.read(offset,sample.data(),sampleLen)) throw std::runtime_error("Cannot read file signature");
                auto kind=identify(sample,sampleLen,path);
                if (!kind.empty()) r.matches.push_back({path,kind,bytes,offset,kind.find("executable")!=std::string::npos || kind.find("ELF")!=std::string::npos || kind.find("PSP")!=std::string::npos});
            }
        }
        r.success=true; r.message="ISO 9660 scan completed";
    } catch (const std::exception& e) { r.message=e.what(); }
    return r;
}
}
