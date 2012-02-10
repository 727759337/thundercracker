#define DECLARE_POOL(classname, maxpool) \
	private: \
	enum { MAX_POOL = maxpool }; \
	static unsigned long allocationMask; \
	static char *allocationPool; \
	public: \
	static void *operator new(size_t size) throw() \
	{ \
		printf("NEW %s\t%x\n", #classname, (unsigned int)allocationMask); \
		static char chunk[MAX_POOL * sizeof(classname)]; \
		if(!allocationPool) allocationPool = chunk; \
        ASSERT(size == sizeof(classname)); \
		for(int bit = 0; bit < MAX_POOL; bit++) \
		{ \
			unsigned long mask = 1 << bit; \
			if(!(mask & allocationMask)) \
			{ \
				allocationMask |= mask; \
				return &allocationPool[bit * sizeof(classname)]; \
			} \
		} \
		return NULL; \
	} \
	static void operator delete(void *p) \
	{ \
		printf("DEL %s\t%x\n", #classname, (unsigned int)allocationMask); \
		if(!p) return; \
		size_t offset = (char*)p - (char*)&allocationPool[0]; \
        ASSERT((offset % sizeof(classname)) == 0); \
		size_t index = offset / sizeof(classname); \
		assert(index < MAX_POOL); \
		allocationMask &= ~(1 << index); \
	}

	

#define DEFINE_POOL(classname) \
	unsigned long classname::allocationMask = 0; \
	char *classname::allocationPool = 0;


