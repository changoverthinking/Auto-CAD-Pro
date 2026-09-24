#ifdef _WIN32
#include "acp/icon_catalog_win.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
namespace acp::icons { namespace {
constexpr std::array<Descriptor,553> kDescriptors{{
#include "icon_catalog_descriptors.inc"
}};
constexpr std::string_view kRleBase64 =
#include "icon_catalog_data.inc"
;
int b64(char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
const std::vector<std::uint8_t>& data(){static const auto d=[](){std::vector<std::uint8_t> o;o.reserve(65536);unsigned v=0;int bits=0;for(char c:kRleBase64){if(c=='=')break;int x=b64(c);if(x<0)continue;v=(v<<6)|static_cast<unsigned>(x);bits+=6;if(bits>=8){bits-=8;o.push_back(static_cast<std::uint8_t>((v>>bits)&0xffu));}}return o;}();return d;}
}
std::size_t count() noexcept{return kDescriptors.size();}
const Descriptor* find(std::string_view name) noexcept{auto i=std::find_if(kDescriptors.begin(),kDescriptors.end(),[&](const Descriptor& d){return d.name==name;});return i==kDescriptors.end()?nullptr:&*i;}
bool draw(HDC dc,std::string_view name,const RECT& area,COLORREF color){const auto*d=find(name);if(!d||!dc)return false;const auto&b=data();if(d->offset+d->length>b.size())return false;std::array<std::uint8_t,576>m{};std::size_t p=0;bool on=false;for(std::size_t i=0;i<d->length&&p<m.size();++i){auto run=b[d->offset+i];for(std::size_t j=0;j<run&&p<m.size();++j)m[p++]=on?1u:0u;on=!on;}if(p!=m.size())return false;int w=std::max(1,(int)(area.right-area.left)),h=std::max(1,(int)(area.bottom-area.top)),s=std::max(1,std::min(w,h)-4),ox=(area.left+area.right-s)/2,oy=(area.top+area.bottom-s)/2;HBRUSH br=CreateSolidBrush(color);if(!br)return false;for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(m[(std::size_t)(y*24+x)]){int x0=ox+x*s/24,x1=ox+((x+1)*s+23)/24,y0=oy+y*s/24,y1=oy+((y+1)*s+23)/24;RECT r{x0,y0,std::max(x0+1,x1),std::max(y0+1,y1)};FillRect(dc,&r,br);}DeleteObject(br);return true;}
}
#endif
