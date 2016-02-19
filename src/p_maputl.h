#ifndef __P_MAPUTL_H
#define __P_MAPUTL_H

#include "r_defs.h"
#include "doomstat.h"

extern int validcount;

struct divline_t
{
	fixed_t 	x;
	fixed_t 	y;
	fixed_t 	dx;
	fixed_t 	dy;
};

struct intercept_t
{
	fixed_t 	frac;			// along trace line
	bool	 	isaline;
	bool		done;
	union {
		AActor *thing;
		line_t *line;
	} d;
};


//==========================================================================
//
// P_PointOnLineSide
//
// Returns 0 (front/on) or 1 (back)
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnLineSide (fixed_t x, fixed_t y, const line_t *line)
{
	extern int P_VanillaPointOnLineSide(fixed_t x, fixed_t y, const line_t* line);

	return i_compatflags2 & COMPATF2_POINTONLINE
		? P_VanillaPointOnLineSide(x, y, line)
		: DMulScale32 (y-line->v1->y, line->dx, line->v1->x-x, line->dy) > 0;
}

inline int P_PointOnLineSidePrecise (fixed_t x, fixed_t y, const line_t *line)
{
	return DMulScale32 (y-line->v1->y, line->dx, line->v1->x-x, line->dy) > 0;
}


//==========================================================================
//
// P_PointOnDivlineSide
//
// Same as P_PointOnLineSide except it uses divlines
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnDivlineSide (fixed_t x, fixed_t y, const divline_t *line)
{
	extern int P_VanillaPointOnDivlineSide(fixed_t x, fixed_t y, const divline_t* line);

	return (i_compatflags2 & COMPATF2_POINTONLINE)
		? P_VanillaPointOnDivlineSide(x, y, line)
		: (DMulScale32 (y-line->y, line->dx, line->x-x, line->dy) > 0);
}

inline int P_PointOnDivlineSidePrecise (fixed_t x, fixed_t y, const divline_t *line)
{
	return DMulScale32 (y-line->y, line->dx, line->x-x, line->dy) > 0;
}


//==========================================================================
//
// P_MakeDivline
//
//==========================================================================

inline void P_MakeDivline (const line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->dx;
	dl->dy = li->dy;
}

struct FLineOpening
{
	fixed_t			top;
	fixed_t			bottom;
	fixed_t			range;
	fixed_t			lowfloor;
	sector_t		*bottomsec;
	sector_t		*topsec;
	FTextureID		ceilingpic;
	FTextureID		floorpic;
	int				floorterrain;
	bool			touchmidtex;
	bool			abovemidtex;
};

void	P_LineOpening (FLineOpening &open, AActor *thing, const line_t *linedef, fixed_t x, fixed_t y, fixed_t refx=FIXED_MIN, fixed_t refy=0, int flags=0);

class FBoundingBox;
struct polyblock_t;

//============================================================================
//
// This is a dynamic array which holds its first MAX_STATIC entries in normal
// variables to avoid constant allocations which this would otherwise
// require.
// 
// When collecting touched portal groups the normal cases are either
// no portals == one group or
// two portals = two groups
// 
// Anything with more can happen but far less infrequently, so this
// organization helps avoiding the overhead from heap allocations
// in the vast majority of situations.
//
//============================================================================

struct FPortalGroupArray
{
	enum
	{
		LOWER = 0x4000,
		UPPER = 0x8000,
		FLAT = 0xc000,
	};

	enum
	{
		MAX_STATIC = 4
	};

	FPortalGroupArray()
	{
		varused = 0;
	}

	void Clear()
	{
		data.Clear();
		varused = 0;
	}

	void Add(DWORD num)
	{
		if (varused < MAX_STATIC) entry[varused++] = (WORD)num;
		else data.Push((WORD)num);
	}

	unsigned Size()
	{
		return varused + data.Size();
	}

	DWORD operator[](unsigned index)
	{
		return index < MAX_STATIC ? entry[index] : data[index - MAX_STATIC];
	}

private:
	WORD entry[MAX_STATIC];
	unsigned varused;
	TArray<WORD> data;
};

class FBlockLinesIterator
{
	friend class FMultiBlockLinesIterator;
	int minx, maxx;
	int miny, maxy;

	int curx, cury;
	polyblock_t *polyLink;
	int polyIndex;
	int *list;

	void StartBlock(int x, int y);

	FBlockLinesIterator() {}
	void init(const FBoundingBox &box);
public:
	FBlockLinesIterator(int minx, int miny, int maxx, int maxy, bool keepvalidcount = false);
	FBlockLinesIterator(const FBoundingBox &box);
	line_t *Next();
	void Reset() { StartBlock(minx, miny); }
};

class FMultiBlockLinesIterator
{
	fixedvec3 checkpoint;
	fixedvec2 offset;
	short basegroup;
	short portalposition;
	WORD index;
	bool continueup;
	bool continuedown;
	FBlockLinesIterator blockIterator;
	FPortalGroupArray checklist;

	void startIteratorForGroup(int group);

public:

	enum
	{
		PP_ORIGIN,
		PP_ABOVE,
		PP_BELOW,
		PP_THROUGHLINE
	};

	struct CheckResult
	{
		line_t *line;
		fixedvec2 position;
		int portalposition;
	};

	FMultiBlockLinesIterator(AActor *origin, fixed_t checkx = FIXED_MAX, fixed_t checky = FIXED_MAX, fixed_t checkradius = -1);
	bool Next(CheckResult *item);
	void Reset();
	// for stopping group traversal through portals. Only the calling code can decide whether this is needed so this needs to be set from the outside.
	void StopUp()
	{
		continueup = false;
	}
	void StopDown()
	{
		continuedown = false;
	}
};


class FBlockThingsIterator
{
	int minx, maxx;
	int miny, maxy;

	int curx, cury;

	FBlockNode *block;

	int Buckets[32];

	struct HashEntry
	{
		AActor *Actor;
		int Next;
	};
	HashEntry FixedHash[10];
	int NumFixedHash;
	TArray<HashEntry> DynHash;

	HashEntry *GetHashEntry(int i) { return i < (int)countof(FixedHash) ? &FixedHash[i] : &DynHash[i - countof(FixedHash)]; }

	void StartBlock(int x, int y);
	void SwitchBlock(int x, int y);
	void ClearHash();

	// The following is only for use in the path traverser 
	// and therefore declared private.
	FBlockThingsIterator();

	friend class FPathTraverse;

public:
	FBlockThingsIterator(int minx, int miny, int maxx, int maxy);
	FBlockThingsIterator(const FBoundingBox &box);
	AActor *Next(bool centeronly = false);
	void Reset() { StartBlock(minx, miny); }
};

class FPathTraverse
{
	static TArray<intercept_t> intercepts;

	divline_t trace;
	unsigned int intercept_index;
	unsigned int intercept_count;
	unsigned int count;

	void AddLineIntercepts(int bx, int by);
	void AddThingIntercepts(int bx, int by, FBlockThingsIterator &it, bool compatible);
public:

	intercept_t *Next();

	FPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags);
	~FPathTraverse();
	const divline_t &Trace() const { return trace; }
};



//
// P_MAPUTL
//

typedef bool(*traverser_t) (intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);


fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1);

#define PT_ADDLINES 	1
#define PT_ADDTHINGS	2
#define PT_COMPATIBLE	4
#define PT_DELTA		8		// x2,y2 is passed as a delta, not as an endpoint


#endif