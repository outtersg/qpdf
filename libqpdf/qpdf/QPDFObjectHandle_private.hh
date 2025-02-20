#ifndef OBJECTHANDLE_PRIVATE_HH
#define OBJECTHANDLE_PRIVATE_HH

#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Stream.hh>

namespace qpdf
{
    class Array final: public BaseHandle
    {
      public:
        explicit Array(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj)
        {
        }

        explicit Array(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj))
        {
        }

        int size() const;
        std::pair<bool, QPDFObjectHandle> at(int n) const;
        bool setAt(int at, QPDFObjectHandle const& oh);
        bool insert(int at, QPDFObjectHandle const& item);
        void push_back(QPDFObjectHandle const& item);
        bool erase(int at);

        std::vector<QPDFObjectHandle> getAsVector() const;
        void setFromVector(std::vector<QPDFObjectHandle> const& items);

      private:
        QPDF_Array* array() const;
        void checkOwnership(QPDFObjectHandle const& item) const;
        QPDFObjectHandle null() const;
    };

    // BaseDictionary is only used as a base class. It does not contain any methods exposed in the
    // public API.
    class BaseDictionary: public BaseHandle
    {
      public:
        // The following methods are not part of the public API.
        bool hasKey(std::string const& key) const;
        QPDFObjectHandle getKey(std::string const& key) const;
        std::set<std::string> getKeys();
        std::map<std::string, QPDFObjectHandle> const& getAsMap() const;
        void removeKey(std::string const& key);
        void replaceKey(std::string const& key, QPDFObjectHandle value);

      protected:
        BaseDictionary() = default;
        BaseDictionary(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj) {};
        BaseDictionary(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj)) {};
        BaseDictionary(BaseDictionary const&) = default;
        BaseDictionary& operator=(BaseDictionary const&) = default;
        BaseDictionary(BaseDictionary&&) = default;
        BaseDictionary& operator=(BaseDictionary&&) = default;
        ~BaseDictionary() = default;

        QPDF_Dictionary* dict() const;
    };

    class Dictionary final: public BaseDictionary
    {
      public:
        explicit Dictionary(std::shared_ptr<QPDFObject> const& obj) :
            BaseDictionary(obj)
        {
        }

        explicit Dictionary(std::shared_ptr<QPDFObject>&& obj) :
            BaseDictionary(std::move(obj))
        {
        }
    };

    class Name final: public BaseHandle
    {
      public:
        // Put # into strings with characters unsuitable for name token
        static std::string normalize(std::string const& name);

        // Check whether name is valid utf-8 and whether it contains characters that require
        // escaping. Return {false, false} if the name is not valid utf-8, otherwise return {true,
        // true} if no characters require or {true, false} if escaping is required.
        static std::pair<bool, bool> analyzeJSONEncoding(std::string const& name);
    };

    class Stream final: public BaseHandle
    {
      public:
        explicit Stream(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj)
        {
        }

        explicit Stream(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj))
        {
        }

        QPDFObjectHandle
        getDict() const
        {
            return stream()->stream_dict;
        }
        bool
        isDataModified() const
        {
            return !stream()->token_filters.empty();
        }
        void
        setFilterOnWrite(bool val)
        {
            stream()->filter_on_write = val;
        }
        bool
        getFilterOnWrite() const
        {
            return stream()->filter_on_write;
        }

        // Methods to help QPDF copy foreign streams
        size_t
        getLength() const
        {
            return stream()->length;
        }
        std::shared_ptr<Buffer>
        getStreamDataBuffer() const
        {
            return stream()->stream_data;
        }
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider>
        getStreamDataProvider() const
        {
            return stream()->stream_provider;
        }

        // See comments in QPDFObjectHandle.hh for these methods.
        bool pipeStreamData(
            Pipeline* p,
            bool* tried_filtering,
            int encode_flags,
            qpdf_stream_decode_level_e decode_level,
            bool suppress_warnings,
            bool will_retry);
        std::shared_ptr<Buffer> getStreamData(qpdf_stream_decode_level_e level);
        std::shared_ptr<Buffer> getRawStreamData();
        void replaceStreamData(
            std::shared_ptr<Buffer> data,
            QPDFObjectHandle const& filter,
            QPDFObjectHandle const& decode_parms);
        void replaceStreamData(
            std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider,
            QPDFObjectHandle const& filter,
            QPDFObjectHandle const& decode_parms);
        void
        addTokenFilter(std::shared_ptr<QPDFObjectHandle::TokenFilter> token_filter)
        {
            stream()->token_filters.emplace_back(token_filter);
        }
        JSON getStreamJSON(
            int json_version,
            qpdf_json_stream_data_e json_data,
            qpdf_stream_decode_level_e decode_level,
            Pipeline* p,
            std::string const& data_filename);
        qpdf_stream_decode_level_e writeStreamJSON(
            int json_version,
            JSON::Writer& jw,
            qpdf_json_stream_data_e json_data,
            qpdf_stream_decode_level_e decode_level,
            Pipeline* p,
            std::string const& data_filename,
            bool no_data_key = false);
        void
        replaceDict(QPDFObjectHandle const& new_dict)
        {
            auto s = stream();
            s->stream_dict = new_dict;
            s->setDictDescription();
        }

        static void registerStreamFilter(
            std::string const& filter_name,
            std::function<std::shared_ptr<QPDFStreamFilter>()> factory);

      private:
        QPDF_Stream*
        stream() const
        {
            if (obj) {
                if (auto s = obj->as<QPDF_Stream>()) {
                    return s;
                }
            }
            throw std::runtime_error("operation for stream attempted on object of type dictionary");
            return nullptr; // unreachable
        }
        bool filterable(
            std::vector<std::shared_ptr<QPDFStreamFilter>>& filters,
            bool& specialized_compression,
            bool& lossy_compression);
        void replaceFilterData(
            QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms, size_t length);

        void warn(std::string const& message);

        static std::map<std::string, std::string> filter_abbreviations;
        static std::map<std::string, std::function<std::shared_ptr<QPDFStreamFilter>()>>
            filter_factories;
    };

    inline qpdf_object_type_e
    BaseHandle::type_code() const
    {
        return obj ? obj->getResolvedTypeCode() : ::ot_uninitialized;
    }

} // namespace qpdf

inline qpdf::Array
QPDFObjectHandle::as_array(qpdf::typed options) const
{
    if (options & qpdf::error) {
        assertType("array", false);
    }
    if (options & qpdf::any_flag || type_code() == ::ot_array ||
        (options & qpdf::optional && type_code() == ::ot_null)) {
        return qpdf::Array(obj);
    }
    return qpdf::Array(std::shared_ptr<QPDFObject>());
}

inline qpdf::Dictionary
QPDFObjectHandle::as_dictionary(qpdf::typed options) const
{
    if (options & qpdf::any_flag || type_code() == ::ot_dictionary ||
        (options & qpdf::optional && type_code() == ::ot_null)) {
        return qpdf::Dictionary(obj);
    }
    if (options & qpdf::error) {
        assertType("dictionary", false);
    }
    return qpdf::Dictionary(std::shared_ptr<QPDFObject>());
}

inline qpdf::Stream
QPDFObjectHandle::as_stream(qpdf::typed options) const
{
    if (options & qpdf::any_flag || type_code() == ::ot_stream ||
        (options & qpdf::optional && type_code() == ::ot_null)) {
        return qpdf::Stream(obj);
    }
    if (options & qpdf::error) {
        assertType("stream", false);
    }
    return qpdf::Stream(std::shared_ptr<QPDFObject>());
}

#endif // OBJECTHANDLE_PRIVATE_HH
