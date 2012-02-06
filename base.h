#ifndef __BASE_H__
#define __BASE_H__

#include <cassert>
#include <string>
#include <uv.h>
#include <http_parser.h>

namespace native
{
	namespace base
	{
		class loop;
		class error;

		typedef uv_req_type req_type;
		typedef uv_membership membership;

		enum uv_callback_id
		{
			uv_cid_close = 0,
			uv_cid_listen,
			uv_cid_read_start,
			uv_cid_write,
			uv_cid_shutdown,
			uv_cid_max
		};

		class exception
		{
		public:
			exception(const std::string& message)
				: message_(message)
			{}

			virtual ~exception() {}

			const std::string& message() const { return message_; }

		private:
			std::string message_;
		};

		class error
		{
		public:
			error(uv_err_t e) : uv_err_(e) {}
			~error() = default;

		public:
			const char* name() const { return uv_err_name(uv_err_); }
			const char* str() const { return uv_strerror(uv_err_); }

		private:
			uv_err_t uv_err_;
		};

		// uv_loop_t wrapper
		class loop
		{
		public:
			loop(bool use_default=false)
				: uv_loop_(use_default ? uv_default_loop() : uv_loop_new())
			{ }

			~loop()
			{
				if(uv_loop_)
				{
					uv_loop_delete(uv_loop_);
					uv_loop_ = nullptr;
				}
			}

			uv_loop_t* get() { return uv_loop_; }

			bool run() { return uv_run(uv_loop_)==0; }
			bool run_once() { return uv_run_once(uv_loop_)==0; }

			void ref() { uv_ref(uv_loop_); }
			void unref() { uv_unref(uv_loop_); }
			void update_time() { uv_update_time(uv_loop_); }
			int64_t now() { return uv_now(uv_loop_); }

			error last_error() { return uv_last_error(uv_loop_); }

		private:
			loop(const loop&);
			void operator =(const loop&);

		private:
			uv_loop_t* uv_loop_;
		};
	}

	int run()
	{
		return uv_run(uv_default_loop());
	}

	int run_once()
	{
		return uv_run(uv_default_loop());
	}

	base::error get_last_error() { return uv_last_error(uv_default_loop()); }
}


#endif
