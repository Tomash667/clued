#pragma once

typedef unsigned char byte;
typedef unsigned int uint;
typedef const char* cstring;

extern string g_tmp_string;

#define BIT(bit) (1<<(bit))
#define IS_SET(flaga,bit) (((flaga) & (bit)) != 0)
#define IS_CLEAR(flaga,bit) (((flaga) & (bit)) == 0)
#define IS_ALL_SET(flaga,bity) (((flaga) & (bity)) == (bity))
#define SET_BIT(flaga,bit) ((flaga) |= (bit))
#define CLEAR_BIT(flaga,bit) ((flaga) &= ~(bit))
#define SET_BIT_VALUE(flaga,bit,wartos) { if(wartos) SET_BIT(flaga,bit); else CLEAR_BIT(flaga,bit); }
#define COPY_BIT(flaga,flaga2,bit) { if(((flaga2) & (bit)) != 0) SET_BIT(flaga,bit); else CLEAR_BIT(flaga,bit); }
#define FLT10(x) (float(int((x)*10))/10)
#define OR2_EQ(var,val1,val2) (((var) == (val1)) || ((var) == (val2)))
#define OR3_EQ(var,val1,val2,val3) (((var) == (val1)) || ((var) == (val2)) || ((var) == (val3)))
// makro na rozmiar tablicy
template <typename T, size_t N>
char ( &_ArraySizeHelper( T (&array)[N] ))[N];
#define countof( array ) (sizeof( _ArraySizeHelper( array ) ))
#define random_string(ss) ((cstring)((ss)[rand2()%countof(ss)]))
#ifndef STRING
#	define _STRING(str) #str
#	define STRING(str) _STRING(str)
#endif

cstring Format(cstring str, ...);
int StringToNumber(cstring s, __int64& i, float& f);
bool LoadFileToString(cstring path, string& str);
void Unescape(const string& sin, string& sout);

template<typename T>
inline T& Add1(vector<T>& v)
{
	v.resize(v.size()+1);
	return v.back();
}

template<typename T>
inline T& Add1(vector<T>* v)
{
	v->resize(v->size()+1);
	return v->back();
}

template<typename T>
inline void DeleteElements(vector<T>& v)
{
	for(vector<T>::iterator it = v.begin(), end = v.end(); it != end; ++it)
		delete *it;
	v.clear();
}

template<typename T>
inline void DeleteElements(vector<T>* v)
{
	DeleteElements(*v);
}

//-----------------------------------------------------------------------------
// kontener u¿ywany na tymczasowe obiekty które s¹ potrzebne od czasu do czasu
//-----------------------------------------------------------------------------
#ifdef _DEBUG
//#	define CHECK_POOL_LEAK
#endif
template<typename T>
struct ObjectPool
{
	~ObjectPool()
	{
		DeleteElements(pool);
#ifdef CHECK_POOL_LEAK
#endif
	}

	inline T* Get()
	{
		T* t;
		if(pool.empty())
			t = new T;
		else
		{
			t = pool.back();
			pool.pop_back();
		}
		return t;
	}

	inline void Free(T* t)
	{
		assert(t);
#ifdef CHECK_POOL_LEAK
		delete t;
#else
		pool.push_back(t);
#endif
	}

	inline void Free(vector<T*>& elems)
	{
		if(elems.empty())
			return;
#ifdef _DEBUG
		for(vector<T*>::iterator it = elems.begin(), end = elems.end(); it != end; ++it)
		{
			assert(*it);
#ifdef CHECK_POOL_LEAK
			delete *it;
#endif
		}
#endif
#ifndef CHECK_POOL_LEAK
		pool.insert(pool.end(), elems.begin(), elems.end());
#endif
		elems.clear();
	}

private:
	vector<T*> pool;
};

// tymczasowe stringi
extern ObjectPool<string> StringPool;
extern ObjectPool<vector<void*> > VectorPool;

//-----------------------------------------------------------------------------
// Lokalny string który wykorzystuje StringPool
//-----------------------------------------------------------------------------
struct LocalString
{
	LocalString()
	{
		s = StringPool.Get();
		s->clear();
	}

	LocalString(cstring str)
	{
		s = StringPool.Get();
		*s = str;
	}

	LocalString(const string& str)
	{
		s = StringPool.Get();
		*s = str;
	}

	~LocalString()
	{
		StringPool.Free(s);
	}

	inline void operator = (cstring str)
	{
		*s = str;
	}

	inline void operator = (const string& str)
	{
		*s = str;
	}

	inline char at_back(uint offset) const
	{
		assert(offset < s->size());
		return s->at(s->size()-1-offset);
	}

	inline void pop(uint count)
	{
		assert(s->size() > count);
		s->resize(s->size()-count);
	}

	inline void operator += (cstring str)
	{
		*s += str;
	}

	inline void operator += (const string& str)
	{
		*s += str;
	}

	inline void operator += (char c)
	{
		*s += c;
	}

	inline operator cstring() const
	{
		return s->c_str();
	}

	inline string& get_ref()
	{
		return *s;
	}

	inline string* get_ptr()
	{
		return s;
	}

	inline string* operator -> ()
	{
		return s;
	}

	inline const string* operator -> () const
	{
		return s;
	}

	inline bool operator == (cstring str) const
	{
		return *s == str;
	}

	inline bool operator == (const string& str) const
	{
		return *s == str;
	}

	inline bool operator == (const LocalString& str) const
	{
		return *s == *str.s;
	}

	inline bool operator != (cstring str) const
	{
		return *s != str;
	}

	inline bool operator != (const string& str) const
	{
		return *s != str;
	}

	inline bool operator != (const LocalString& str) const
	{
		return *s != *str.s;
	}

private:
	string* s;
};

//-----------------------------------------------------------------------------
// Lokalny wektor przechowuj¹cy wskaŸniki
//-----------------------------------------------------------------------------
template<typename T>
struct LocalVector
{
	LocalVector()
	{
		v = (vector<T>*)VectorPool.Get();
		v->clear();
	}

	LocalVector(vector<T>& v2)
	{
		v = (vector<T>*)VectorPool.Get();
		*v = v2;
	}

	~LocalVector()
	{
		VectorPool.Free((vector<void*>*)v);
	}

	inline operator vector<T>& ()
	{
		return *v;
	}

	inline vector<T>* operator -> ()
	{
		return v;
	}

	inline const vector<T>* operator -> () const
	{
		return v;
	}

	inline T& operator [] (int n)
	{
		return v->at(n);
	}

	inline void Shuffle()
	{
		std::random_shuffle(v->begin(), v->end(), myrand);
	}

private:
	vector<T>* v;
};

template<typename T>
struct LocalVector2
{
	typedef vector<T> Vector;
	typedef Vector* VectorPtr;
	typedef typename Vector::iterator Iter;

	LocalVector2()
	{
		v = (VectorPtr)VectorPool.Get();
		v->clear();
	}

	LocalVector2(Vector& v2)
	{
		v = (VectorPtr*)VectorPool.Get();
		*v = v2;
	}

	~LocalVector2()
	{
		VectorPool.Free((vector<void*>*)v);
	}

	inline void push_back(T e)
	{
		v->push_back(e);
	}

	inline bool empty() const
	{
		return v->empty();
	}

	inline Iter begin()
	{
		return v->begin();
	}

	inline Iter end()
	{
		return v->end();
	}

	inline uint size() const
	{
		return v->size();
	}

	//T& random_item()
	//{
	//	return v->at(rand2()%v->size());
	//}

private:
	VectorPtr v;
};

#undef ERROR
#define ERROR(x) printf("ERROR: %s", x)

#include "Tokenizer.h"
