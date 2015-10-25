#pragma once
namespace std {

    // print std::tuple
    // 0
    inline std::ostream &operator<<(std::ostream &os, const std::tuple<> &t)
    {
        os << "()";
        return os;
    }

    // 1
    template<typename A1>
        inline std::ostream &operator<<(std::ostream &os, const std::tuple<A1> &t)
        {
            os 
                << "("
                << std::get<0>(t)
                << ")";
            return os;
        }

    // 2
    template<typename A1, typename A2>
        inline std::ostream &operator<<(std::ostream &os, const std::tuple<A1, A2> &t)
        {
            os 
                << "("
                << std::get<0>(t)
                << ", " << std::get<1>(t)
                << ")";
            return os;
        }

    // 3
    template<typename A1, typename A2, typename A3>
        inline std::ostream &operator<<(std::ostream &os, const std::tuple<A1, A2, A3> &t)
        {
            os 
                << "("
                << std::get<0>(t)
                << ", " << std::get<1>(t)
                << ", " << std::get<2>(t)
                << ")";
            return os;
        }

    // 4
    template<typename A1, typename A2, typename A3, typename A4>
        inline std::ostream &operator<<(std::ostream &os, const std::tuple<A1, A2, A3, A4> &t)
        {
            os 
                << "("
                << std::get<0>(t)
                << ", " << std::get<1>(t)
                << ", " << std::get<2>(t)
                << ", " << std::get<3>(t)
                << ")";
            return os;
        }


    // call_with_tuple
    // 0
    template<typename F>
        auto call_with_tuple(const F &handler, const std::tuple<> &args)->decltype(handler())
        {
            return handler();
        }

    // 1
    template<typename F, typename A1>
        auto call_with_tuple(const F &handler, const std::tuple<A1> &args)->decltype(handler(A1()))
        {
            return handler(std::get<0>(args));
        }

    // 2
    template<typename F, typename A1, typename A2>
        auto call_with_tuple(const F &handler, const std::tuple<A1, A2> &args)->decltype(handler(A1(), A2()))
        {
            return handler(std::get<0>(args), std::get<1>(args));
        }

    // 3
    template<typename F, typename A1, typename A2, typename A3>
        auto call_with_tuple(const F &handler, const std::tuple<A1, A2, A3> &args)->decltype(handler(A1(), A2(), A3()))
        {
            return handler(std::get<0>(args), std::get<1>(args), std::get<2>(args));
        }

    // 4
    template<typename F, typename A1, typename A2, typename A3, typename A4>
        auto call_with_tuple(const F &handler, const std::tuple<A1, A2, A3, A4> &args)->decltype(handler(A1(), A2(), A3(), A4()))
        {
            return handler(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args));
        }

    // call_with_tuple_void
    // 0
    template<typename F>
        void call_with_tuple_void(const F &handler, const std::tuple<> &args)
        {
            handler();
        }
    // 1
    template<typename F, typename A1>
        void call_with_tuple_void(const F &handler, const std::tuple<A1> &args)
        {
            handler(std::get<0>(args));
        }
    // 2
    template<typename F, typename A1, typename A2>
        void call_with_tuple_void(const F &handler, const std::tuple<A1, A2> &args)
        {
            handler(std::get<0>(args), std::get<1>(args));
        }
    // 3
    template<typename F, typename A1, typename A2, typename A3>
        void call_with_tuple_void(const F &handler, const std::tuple<A1, A2, A3> &args)
        {
            handler(std::get<0>(args), std::get<1>(args), std::get<2>(args));
        }
    // 4
    template<typename F, typename A1, typename A2, typename A3, typename A4>
        void call_with_tuple_void(const F &handler, const std::tuple<A1, A2, A3, A4> &args)
        {
            handler(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args));
        }
}
