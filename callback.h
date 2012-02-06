#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include <memory>
#include <utility>
#include <vector>
#include "base.h"

namespace native
{
	namespace base
	{
		enum callback_id
		{
			cid_close = 0,
			cid_listen,
			cid_read_start,
			cid_write,
			cid_max
		};

		namespace callback_serialization
		{
			class callback_object_base
			{
			public:
				callback_object_base(void* data)
					: data_(data)
				{
					//printf("callback_object_base(): %x\n", this);
				}
				virtual ~callback_object_base()
				{
					//printf("~callback_object_base(): %x\n", this);
				}

				void* get_data() { return data_; }

			private:
				void* data_;
			};

			// SCO: serialized callback object
			template<typename callback_t>
			class callback_object : public callback_object_base
			{
			public:
				callback_object(const callback_t& callback, void* data)
					: callback_object_base(data)
					, callback_(callback)
				{
					//printf("callback_object<>(): %x\n", this);
				}

				virtual ~callback_object()
				{
					//printf("~callback_object<>(): %x\n", this);
				}

			public:
				template<typename ...A>
				void invoke(A&& ... args)
				{
					callback_(std::forward<A>(args)...);
				}

			private:
				callback_t callback_;
			};
		}

		typedef std::shared_ptr<callback_serialization::callback_object_base> callback_object_ptr;

		class callbacks
		{
		public:
			callbacks()
				: lut_(cid_max)
			{
				//printf("callbacks(): %x\n", this);
			}
			~callbacks()
			{
				//printf("~callbacks(): %x\n", this);
			}

			template<typename callback_t>
			static void store(void* target, callback_id cid, const callback_t& callback, void* data=nullptr)
			{
				reinterpret_cast<callbacks*>(target)->lut_[cid] = callback_object_ptr(new callback_serialization::callback_object<callback_t>(callback, data));
			}

			template<typename callback_t>
			static void get_data(void* target, callback_id cid)
			{
				return reinterpret_cast<callbacks*>(target)->lut_[cid]->get_data();
			}

			template<typename callback_t, typename ...A>
			static void invoke(void* target, callback_id cid, A&& ... args)
			{
				auto x = dynamic_cast<callback_serialization::callback_object<callback_t>*>(reinterpret_cast<callbacks*>(target)->lut_[cid].get());
				assert(x);
				x->invoke(args...);
			}

		private:
			std::vector<callback_object_ptr> lut_;
		};
	}
}

#endif
