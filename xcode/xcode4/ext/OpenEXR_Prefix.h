

// The only way I can find to force __SSE2__ off is to use this
// file as a prefix header.  Need to do this in Xcode 3 for 32-bit

#if !__LP64__
	#undef __SSE2__
#endif
