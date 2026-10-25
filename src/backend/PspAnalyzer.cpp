#include "PspAnalyzer.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace DecompileX {
namespace {
using Bytes = std::vector<std::uint8_t>;
struct View {
    const Bytes& b;
    std::size_t start, size;
    bool valid(std::uint64_t p, std::uint64_t n) const { return p <= size && n <= size - p; }
    std::uint8_t u8(std::uint64_t p) const { if (!valid(p,1)) throw std::runtime_error("Truncated binary"); return b[start + static_cast<std::size_t>(p)]; }
    std::uint16_t u16(std::uint64_t p, bool le) const { auto a=u8(p),c=u8(p+1); return le ? static_cast<std::uint16_t>(a | (c<<8)) : static_cast<std::uint16_t>((a<<8)|c); }
    std::uint32_t u32(std::uint64_t p, bool le) const { auto a=u16(p,le),c=u16(p+2,le); return le ? static_cast<std::uint32_t>(a | (static_cast<std::uint32_t>(c)<<16)) : static_cast<std::uint32_t>((static_cast<std::uint32_t>(a)<<16)|c); }
};
std::string hex(std::uint64_t x, int digits=8) { std::ostringstream s; s<<"0x"<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(digits)<<x; return s.str(); }
const char* reg(unsigned r) { static constexpr const char* names[]={"zero","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7","s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"}; return names[r&31]; }
std::string disasm(std::uint32_t w, std::uint32_t pc) {
    unsigned op=w>>26, rs=(w>>21)&31, rt=(w>>16)&31, rd=(w>>11)&31, sh=(w>>6)&31, fn=w&63;
    std::int32_t imm=static_cast<std::int16_t>(w&65535);
    std::ostringstream s;
    auto r3=[&](const char* name){s<<name<<" $"<<reg(rd)<<", $"<<reg(rs)<<", $"<<reg(rt);};
    auto r2=[&](const char* name){s<<name<<" $"<<reg(rs)<<", $"<<reg(rt);};
    auto i3=[&](const char* name){s<<name<<" $"<<reg(rt)<<", $"<<reg(rs)<<", "<<imm;};
    auto mem=[&](const char* name){s<<name<<" $"<<reg(rt)<<", "<<imm<<"($"<<reg(rs)<<")";};
    auto branch=[&](const char* name){r2(name); s<<", "<<hex(static_cast<std::uint32_t>(pc+4+static_cast<std::uint32_t>(imm*4)));};
    if (w==0) return "nop";
    if (op==0) {
        switch(fn) {
        case 0: s<<"sll $"<<reg(rd)<<", $"<<reg(rt)<<", "<<sh; break;
        case 2: s<<"srl $"<<reg(rd)<<", $"<<reg(rt)<<", "<<sh; break;
        case 3: s<<"sra $"<<reg(rd)<<", $"<<reg(rt)<<", "<<sh; break;
        case 4: r3("sllv"); break;
        case 6: r3("srlv"); break;
        case 7: r3("srav"); break;
        case 8: s<<"jr $"<<reg(rs); break;
        case 9: s<<"jalr $"<<reg(rd)<<", $"<<reg(rs); break;
        case 12: s<<"syscall"; break;
        case 13: s<<"break"; break;
        case 16: s<<"mfhi $"<<reg(rd); break;
        case 17: s<<"mthi $"<<reg(rs); break;
        case 18: s<<"mflo $"<<reg(rd); break;
        case 19: s<<"mtlo $"<<reg(rs); break;
        case 24: r2("mult"); break;
        case 25: r2("multu"); break;
        case 26: r2("div"); break;
        case 27: r2("divu"); break;
        case 32: r3("add"); break;
        case 33: r3("addu"); break;
        case 34: r3("sub"); break;
        case 35: r3("subu"); break;
        case 36: r3("and"); break;
        case 37: r3("or"); break;
        case 38: r3("xor"); break;
        case 39: r3("nor"); break;
        case 42: r3("slt"); break;
        case 43: r3("sltu"); break;
        default: s<<".word "<<hex(w); }
    } else switch(op) {
    case 2: case 3: s<<(op==2?"j ":"jal ")<<hex(((pc+4)&0xF0000000u)|((w&0x03FFFFFFu)<<2)); break;
    case 4: branch("beq"); break;
    case 5: branch("bne"); break;
    case 6: s<<"blez $"<<reg(rs)<<", "<<hex(static_cast<std::uint32_t>(pc+4+imm*4)); break;
    case 7: s<<"bgtz $"<<reg(rs)<<", "<<hex(static_cast<std::uint32_t>(pc+4+imm*4)); break;
    case 8: i3("addi"); break;
    case 9: i3("addiu"); break;
    case 10: i3("slti"); break;
    case 11: i3("sltiu"); break;
    case 12: s<<"andi $"<<reg(rt)<<", $"<<reg(rs)<<", "<<hex(w&65535,4); break;
    case 13: s<<"ori $"<<reg(rt)<<", $"<<reg(rs)<<", "<<hex(w&65535,4); break;
    case 14: s<<"xori $"<<reg(rt)<<", $"<<reg(rs)<<", "<<hex(w&65535,4); break;
    case 15: s<<"lui $"<<reg(rt)<<", "<<hex(w&65535,4); break;
    case 32: mem("lb"); break;
    case 33: mem("lh"); break;
    case 35: mem("lw"); break;
    case 36: mem("lbu"); break;
    case 37: mem("lhu"); break;
    case 40: mem("sb"); break;
    case 41: mem("sh"); break;
    case 43: mem("sw"); break;
    default: s<<".word "<<hex(w); }
    return s.str();
}
std::string elfReport(const View& v, std::size_t limit) {
    if (!v.valid(0,52) || v.u8(0)!=0x7F || v.u8(1)!='E' || v.u8(2)!='L' || v.u8(3)!='F') return "Not an ELF executable (possibly encrypted PSP PRX).";
    if (v.u8(4)!=1) return "ELF detected, but only ELF32 is supported.";
    if (v.u8(5)!=1 && v.u8(5)!=2) return "Unsupported ELF byte order.";
    bool le=v.u8(5)==1;
    std::ostringstream out;
    std::uint16_t machine=v.u16(18,le), type=v.u16(16,le);
    std::uint32_t entry=v.u32(24,le), phoff=v.u32(28,le), shoff=v.u32(32,le);
    std::uint16_t phentsize=v.u16(42,le), phnum=v.u16(44,le), shentsize=v.u16(46,le), shnum=v.u16(48,le), shstr=v.u16(50,le);
    out<<"ELF32 "<<(le?"little-endian":"big-endian")<<"\nMachine: "<<machine<<(machine==8?" (MIPS)":" (not MIPS)")<<"\nType: "<<type<<"\nEntry: "<<hex(entry)<<"\n";
    if (machine!=8) return out.str()+"MIPS disassembly skipped: different architecture.\n";
    if (phnum>0 && (phentsize<32 || !v.valid(phoff,static_cast<std::uint64_t>(phnum)*phentsize))) return out.str()+"Invalid ELF program headers.\n";
    if (shnum>0 && (shentsize<40 || !v.valid(shoff,static_cast<std::uint64_t>(shnum)*shentsize))) return out.str()+"Invalid ELF section headers.\n";
    out<<"Program headers: "<<phnum<<"\nSections: "<<shnum<<"\n";
    std::uint32_t codeOffset=0, codeSize=0, codeAddr=entry;
    bool found=false;
    for (std::uint32_t i=0;i<phnum;++i) {
        auto p=static_cast<std::uint64_t>(phoff)+i*phentsize;
        std::uint32_t pt=v.u32(p,le), off=v.u32(p+4,le), va=v.u32(p+8,le), fs=v.u32(p+16,le), flags=v.u32(p+24,le);
        if (pt==1 && (flags&1) && v.valid(off,fs)) {
            out<<"Executable LOAD: file="<<hex(off)<<" vaddr="<<hex(va)<<" bytes="<<fs<<"\n";
            if (!found || (entry>=va && static_cast<std::uint64_t>(entry)-va<fs)) {
                codeOffset=off; codeSize=fs; codeAddr=va; found=true;
                if (entry>=va && static_cast<std::uint64_t>(entry)-va<fs) {codeOffset+=entry-va;codeSize-=entry-va;codeAddr=entry;break;}
            }
        }
    }
    if (!found) { out<<"No valid executable LOAD segment found.\n"; return out.str(); }
    if (shnum>0 && shstr<shnum) {
        auto p=static_cast<std::uint64_t>(shoff)+shstr*shentsize;
        auto strOff=v.u32(p+16,le),strSize=v.u32(p+20,le);
        if (v.valid(strOff,strSize)) {
            out<<"Section names: "; std::size_t printed=0;
            for (std::uint32_t i=0;i<shnum && printed<24;++i) {
                auto q=static_cast<std::uint64_t>(shoff)+i*shentsize;
                auto no=v.u32(q,le);
                if (no>=strSize) continue;
                std::string name;
                for (std::uint64_t j=strOff+no;j<static_cast<std::uint64_t>(strOff)+strSize && name.size()<80;++j) {char c=static_cast<char>(v.u8(j));if (!c) break;name+=c;}
                if (!name.empty()) {out<<name<<" ";++printed;}
            }
            out<<"\n";
        }
    }
    out<<"\nMIPS DISASSEMBLY (subset; entry/exec segment)\n";
    std::size_t count=std::min<std::size_t>(limit,codeSize/4);
    for (std::size_t i=0;i<count;++i) {
        auto w=v.u32(static_cast<std::uint64_t>(codeOffset)+i*4,le);
        out<<hex(static_cast<std::uint32_t>(codeAddr+static_cast<std::uint32_t>(i*4)))<<"  "<<hex(w)<<"  "<<disasm(w,static_cast<std::uint32_t>(codeAddr+i*4))<<"\n";
    }
    if (count*4<codeSize) out<<"... output limited to "<<count<<" instructions.\n";
    out<<"This is partial assembly, not recovered C/C++ source.\n";
    return out.str();
}
}
std::string PspAnalyzer::analyze(const std::vector<std::uint8_t>& input, std::size_t maxInstructions) {
    try {
        if (input.empty()) return "Empty file.";
        View v{input,0,input.size()};
        if (v.valid(0,4) && v.u8(0)==0 && v.u8(1)=='P' && v.u8(2)=='B' && v.u8(3)=='P') {
            if (!v.valid(0,40)) return "Truncated PBP header.";
            static constexpr const char* labels[]={"PARAM.SFO","ICON0.PNG","ICON1.PMF","PIC0.PNG","PIC1.PNG","SND0.AT3","DATA.PSP","DATA.PSAR"};
            std::ostringstream out; out<<"PSP PBP container\nVersion: "<<hex(v.u32(4,true))<<"\n";
            std::array<std::uint32_t,8> offsets{};
            for (int i=0;i<8;++i) offsets[i]=v.u32(8+i*4,true);
            for (int i=0;i<8;++i) {
                std::uint64_t begin=offsets[i], end=i==7?static_cast<std::uint64_t>(v.size):offsets[i+1];
                if (begin<40 || end<begin || end>v.size) return out.str()+"Invalid PBP section offsets.\n";
                out<<labels[i]<<": "<<end-begin<<" bytes\n";
            }
            auto begin=offsets[6],end=offsets[7];
            if (end==begin) return out.str()+"No DATA.PSP payload.\n";
            View payload{input,begin,end-begin};
            if (payload.valid(0,4) && payload.u8(0)==0x7F && payload.u8(1)=='E') return out.str()+"\n"+elfReport(payload,maxInstructions);
            if (payload.valid(0,4) && payload.u8(0)=='~' && payload.u8(1)=='P' && payload.u8(2)=='S' && payload.u8(3)=='P') return out.str()+"\nEncrypted/packed ~PSP module. Decryption not supported.\n";
            return out.str()+"\nDATA.PSP is not a plain ELF; disassembly unavailable.\n";
        }
        if (v.valid(0,4) && v.u8(0)=='~' && v.u8(1)=='P' && v.u8(2)=='S' && v.u8(3)=='P') return "Encrypted/packed PSP module (~PSP). Decryption not supported.";
        return elfReport(v,maxInstructions);
    } catch (const std::exception& e) { return std::string("Invalid PSP/ELF input: ")+e.what(); }
}
}
