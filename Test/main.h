
const bool enable_repeat_times = false;

const bool enable_connect_close = true;
const bool enable_sync_call = true;
const bool enable_async_oneway = true;
const bool enable_async_twoway = true;


const bool enable_getset_uuid = true;
const bool enable_sync_getset_uuid = true;
const bool enable_async_getset_uuid = true;


const bool enable_exception = true;


using namespace boost::asio::ip;
struct F
{
	F()
	{
		BOOST_TEST_MESSAGE("setup fixture");
	}

	~F() { BOOST_TEST_MESSAGE("teardown fixture"); }
};

static auto peer = tcp::endpoint(address::from_string("127.0.0.1"), 8070);
static int done = 0;

int clientadd(int a, int b);