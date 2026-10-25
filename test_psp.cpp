#include "src/backend/PspAnalyzer.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
int main() {
 std::vector<std::uint8_t> elf(0x120,0);
 elf[0]=0x7f;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=1;elf[5]=1;elf[6]=1;
 auto w16=[&](int p,int v){elf[p]=v&255;elf[p+1]=(v>>8)&255;};
 auto w32=[&](int p,std::uint32_t v){for(int i=0;i<4;i++)elf[p+i]=(v>>(8*i))&255;};
 w16(16,2);w16(18,8);w32(24,0x8800000);w32(28,52);w16(42,32);w16(44,1);
 w32(52,1);w32(56,0x100);w32(60,0x8800000);w32(68,8);w32(76,5);
 w32(0x100,0x24020001);w32(0x104,0x03e00008);
 auto report=DecompileX::PspAnalyzer::analyze(elf);
 std::cout<<report;
 assert(report.find("addiu $v0, $zero, 1")!=std::string::npos);
 assert(report.find("jr $ra")!=std::string::npos);
 std::vector<std::uint8_t> pbp(40+elf.size()); pbp[1]='P';pbp[2]='B';pbp[3]='P';
 for(int i=0;i<6;i++)pbp[8+i*4]=40;
 pbp[32]=40;
 int end=static_cast<int>(pbp.size());for(int i=0;i<4;i++)pbp[36+i]=(end>>(8*i))&255;
 std::copy(elf.begin(),elf.end(),pbp.begin()+40);
 auto pbpr=DecompileX::PspAnalyzer::analyze(pbp);
 assert(pbpr.find("PSP PBP container")!=std::string::npos);
 assert(pbpr.find("jr $ra")!=std::string::npos);
 assert(DecompileX::PspAnalyzer::analyze({1,2,3}).find("Not an ELF")!=std::string::npos);
}
