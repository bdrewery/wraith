/* this code isnt mine. */

#ifndef _PTRLIST_H
#define _PTRLIST_H

//template <class T> class ptrlist;

#define PFOR(_init, _type, _ptr) \
ptrlist<_type>::link *_p;\
for (_p = (_init)->start(), _ptr = ((_type *) _p->ptr()); _p; _p = _p->next(), _ptr = _p ? ((_type *) _p->ptr()) : NULL)

/* Usage:
  Member *m = NULL;

  PFOR(chan->channel.hmember, Member, m) {
    m->dump_idx(idx);
  }
*/

template <class T> class ptrlist
{
	public:
	class link
	{
		private:
		T *_ptr;
		link *_next;
		link *_prev;

		public:
		link *next()	{ return _next; };
		T *ptr()		{ return _ptr; };
		link(T *p)		{ _ptr = p; };
		friend class ptrlist;
	};

	private:
	link *first;
	int ent;
	int _removePtrs;

	public:
	////////////////////////////////////////
	int expire(time_t t, time_t now)
	{
		link *q, *p = first;
		while(p)
		{
			if(p->ptr()->creation() + t <= now)
			{
				q = p->next();
				remove(p);
				p = q;
			}
			else p = p->next();
		}
		return ent;
	}

	////////////////////////////////////////
	int entries()			{ return ent; };

	////////////////////////////////////////
	// returns pointer to a first link
	link *start()	{ return first; };

	////////////////////////////////////////
	// enable deletion of ptr() on removal
	// of a link
	void removePtrs()
	{
		_removePtrs = 1;
	}

	////////////////////////////////////////
	// gets pointer to n-th link
	link *getItem(int num)
	{
		link *p;
		int i;

		if(num + 1 > ent) return NULL;
		p = first;
		i = 0;

		while(p)
		{
			if(i == num) return p;
			p = p->_next;
			i++;
		}
		return NULL;
	}

	//////////////////////////////////////////////////
	// finds object
	// to perform such operation one has to create
	// object which will be identical to one you
	// want to remove (in aspect of == operator, ofc)
	// and then pass it as argument to this fucntion
	link *find(T &obj)
	{
		link *p = first;

		while(p)
		{
			if(obj == *p->_ptr) return p;
			p = p->_next;
		}
		return NULL;
	}

	//////////////////////////////////////
	// find by pointer
	link *find(T *ptr)
	{
		link *p = first;

		while(p)
		{
			if(ptr == p->_ptr) return p;
			p = p->_next;
		}
		return NULL;
	}

	//////////////////////////////
	// remove by link
	int removeLink(link *p)
	{
		if(first == p)
		{
			first = first->_next;
			if(first) first->_prev = NULL;
			if(_removePtrs) delete p->ptr();
			delete p;
			--ent;
			return 1;
		}
		else
		{
			p->_prev->_next = p->_next;
			if(p->_next) p->_next->_prev = p->_prev;
			if(_removePtrs) delete p->ptr();
			delete p;
			--ent;
			return 1;
		}
		return 0;
	}
	/////////////////////////////////
	// remove by object
	int remove(T &obj)
	{
		link *p = first;

		if(!ent) return 0;
		if(*first->_ptr == obj)
		{
			first = first->_next;
			if(first) first->_prev = NULL;
			if(_removePtrs) delete p->ptr();
			delete p;
			--ent;
			return 1;
		}
		else
		{
			while(p)
			{
				if(*p->_ptr == obj)
				{
					p->_prev->_next = p->_next;
					if(p->_next) p->_next->_prev = p->_prev;
					if(_removePtrs) delete p->ptr();
					delete p;
					--ent;
					return 1;
				}
				p = p->_next;
			}
		}
		return 0;
	}
	//////////////////////////////
	// remove by pointer
	int remove(T *ptr)
	{
		link *p = first;

		if(!ent) return 0;
		if(first->_ptr == ptr)
		{
			first = first->_next;
			if(first) first->_prev = NULL;
			if(_removePtrs) delete p->ptr();
			delete p;
			--ent;
			return 1;
		}
		else
		{
			while(p)
			{
				if(p->_ptr == ptr)
				{
					p->_prev->_next = p->_next;
					if(p->_next) p->_next->_prev = p->_prev;
					if(_removePtrs) delete p->ptr();
					delete p;
					--ent;
					return 1;
				}
				p = p->_next;
			}
		}
		return 0;
	}

	//////////////////////////////////
	// sort add pointer to a list
	void sortAdd(T *ptr)
	{
		link *p, *q;

		if(!ptr)  return;

		if(!ent)
		{
			first = new link(ptr);
			first->_next = first->_prev = NULL;
			++ent;
			return;
		}
		else
		{
			//if(strcmp(ptr->nick, first->ptr->nick) < 0)
			if(*ptr < *first->_ptr)
  			{
				q = new link(ptr);
				first->_prev = q;
				q->_prev = NULL;
				q->_next = first;
				first = q;
				++ent;
				return;
			}
			else
			{
				p = first;
				while(1)
				{
					//if(!strcmp(ptr->nick, p->ptr->nick)) return;
					//if(strcmp(ptr->nick, p->ptr->nick) < 0)

					if(*ptr == *p->_ptr) return;
					if(*ptr < *p->_ptr)
					{
						q = new link(ptr);
						q->_next = p;
						q->_prev = p->_prev;
						p->_prev->_next = q;
						p->_prev = q;
						++ent;
						return;
					}
					else if(p->_next == NULL)
					{
						q = new link(ptr);
						q->_next = NULL;
						q->_prev = p;
						p->_next = q;
						++ent;
						return;
					}
					p = p->_next;
				}
			}
		}
	}

	//////////////////////////////
	// add to begining of a list
	void add(T *ptr)
	{
		link *p;

		if(!ent)
		{
			first = new link(ptr);
			first->_next = first->_prev = NULL;
		}
		else
		{
			p = first->_prev = new link(ptr);
			p->_next = first;
			p->_prev = NULL;
			first = p;
		}
		++ent;
	}

	//////////////////////////////////
	// add to the end of a list
	void addLast(T *ptr)
	{
		if(!ent)
		{
			first = new link(ptr);
			first->_next = first->_prev = NULL;
		}
		else
		{
			link *p = first;

			while(p->_next)
				p = p->_next;

			p->_next = new link(ptr);
			p->_next->_prev = p;
			p->_next->_next = NULL;
		}
		++ent;
	}

	//////////////////////
	// constructor
	ptrlist()
	{
		first = NULL;
		ent = 0;
		_removePtrs = 0;
	}

    //////////////////////
	// destructor
	~ptrlist()
	{
		link *p = first;
		link *q;

		while(p)
		{
			q = p;
			p = p->_next;
			if(_removePtrs) delete q->ptr();
			delete(q);
		}
	}

	/////////////////////////////
	// clean all entries
	void reset()
	{
		this->~ptrlist();

		first = NULL;
		ent = 0;
		_removePtrs = 0;
	}
	////////////////////////////
#ifdef HAVE_DEBUG
	void display()
	{
		link *p = first;
		while(p)
		{
			p->ptr()->display();
			p = p->_next;
		}
	}
#endif
};
#endif /* !_PTRLIST_H */
