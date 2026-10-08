#pragma once
namespace hdr { template<class T> T clamp(T v,T lo,T hi){ return v<lo?lo:(v>hi?hi:v);} }
