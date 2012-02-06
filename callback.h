#ifndef CALLBACK_H
#define CALLBACK_H

namespace native
{
	namespace base
	{
		enum callback_id
		{
			cid_close = 0,
			cid_listen,
			cid_read_start,
			cid_max
		};

		namespace callback_serialization
		{
			class callback_object_base
			{
			public:
				callback_object_base()
				{
					printf("callback_object_base(): %x\n", this);
				}
				virtual ~callback_object_base()
				{
					printf("~callback_object_base(): %x\n", this);
				}
			};

			// SCO: serialized callback object
			template<typename callback_t>
			class callback_object : public callback_object_base
			{
			public:
				callback_object(const callback_t& callback, void* data)
					: callback_(callback), data_(data)
				{
					printf("callback_object<>(): %x\n", this);
				}

				virtual ~callback_object()
				{
					printf("~callback_object<>(): %x\n", this);
				}

			public:
				template<typename ...A>
				void invoke(A&& ... args)
				{
					callback_(std::forward<A>(args)...);
				}

			private:
				callback_t callback_;
				void* data_;
			};
		}

		using namespace callback_serialization;

		class callbacks
		{
		public:
			callbacks()
				: lut_(cid_max)
			{
				printf("callbacks(): %x\n", this);
			}
			~callbacks()
			{
				printf("~callbacks(): %x\n", this);
				for(auto i:lut_) {
					delete i;
				}
				lut_.clear();
			}

			template<typename callback_t, typename T>
			static void store(T* target, callback_id cid, const callback_t& callback, void* data=nullptr)
			{
				reinterpret_cast<callbacks*>(target->data)->lut_[cid] = new callback_object<callback_t>(callback, data);
			}

			template<typename callback_t, typename T, typename ...A>
			static void invoke(T* target, callback_id cid, A&& ... args)
			{
				auto x = dynamic_cast<callback_object<callback_t>*>(reinterpret_cast<callbacks*>(target->data)->lut_[cid]);
				assert(x);
				x->invoke(args...);
			}

		private:
			std::vector<callback_object_base*> lut_;
		};
	}
}

#endif
