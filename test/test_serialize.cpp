#include <algorithm>
#include "server/log.h"
#include "server/server.h"

static auto g_logger = ROOT_LOGGER;

void Test() {  // NOLINT
/*
 * 测试用例设计：
 * 随机生成长度为len，类型为type的数组，调用Write_fun将这个数组全部写入块大小为base_len的ByteArray对象中，
 * 再将ByteArray的当前操作位置重设为0，也就是从起点开始，用Read_fun重复读取数据，并与写入的数据比较，
 * 当读取出的数据与写入的数据全部相等时，该测试用例通过
 */
#define XX(type, len, Write_fun, Read_fun, base_len)                                                          \
  {                                                                                                           \
    std::vector<type> vec;                                                                                    \
    vec.reserve(len);                                                                                         \
    for (int i = 0; i < (len); ++i) {                                                                         \
      vec.push_back(rand());                                                                                  \
    }                                                                                                         \
    wtsclwq::ByteArray::s_ptr ba(new wtsclwq::ByteArray(base_len));                                           \
    for (auto &i : vec) {                                                                                     \
      ba->Write_fun(i);                                                                                       \
    }                                                                                                         \
    ba->SetPosition(0);                                                                                       \
    for (size_t i = 0; i < vec.size(); ++i) {                                                                 \
      type v = ba->Read_fun();                                                                                \
      ASSERT(v == vec[i]);                                                                                    \
    }                                                                                                         \
    ASSERT(ba->GetReadSize() == 0);                                                                           \
    LOG_INFO(g_logger) << #Write_fun "/" #Read_fun " (" #type ") len=" << (len) << " base_len=" << (base_len) \
                       << " size=" << ba->GetSize();                                                          \
  }

  XX(int8_t, 100, WriteFint8, ReadFint8, 1);
  XX(uint8_t, 100, WriteFuint8, ReadFuint8, 1);
  XX(int16_t, 100, WriteFint16, ReadFint16, 1);
  XX(uint16_t, 100, WriteFuint16, ReadFuint16, 1);
  XX(int32_t, 100, WriteFint32, ReadFint32, 1);
  XX(uint32_t, 100, WriteFuint32, ReadFuint32, 1);
  XX(int64_t, 100, WriteFint64, ReadFint64, 1);
  XX(uint64_t, 100, WriteFuint64, ReadFuint64, 1);

  XX(int32_t, 100, WriteInt32, ReadInt32, 1);
  XX(uint32_t, 100, WriteUint32, ReadUint32, 1);
  XX(int64_t, 100, WriteInt64, ReadInt64, 1);
  XX(uint64_t, 100, WriteUint64, ReadUint64, 1);
#undef XX

/*
 * 测试用例设计：
 * 在前面的测试用例基础上，增加文件序列化和反序列化操作，
 * 当写入文件的内容与从文件读取出的内容完全一致时，测试用例通过
 */
#define XX(type, len, Write_fun, Read_fun, base_len)                                                          \
  {                                                                                                           \
    std::vector<type> vec;                                                                                    \
    vec.reserve(len);                                                                                         \
    for (int i = 0; i < (len); ++i) {                                                                         \
      vec.push_back(rand());                                                                                  \
    }                                                                                                         \
    wtsclwq::ByteArray::s_ptr ba(new wtsclwq::ByteArray(base_len));                                           \
    for (auto &i : vec) {                                                                                     \
      ba->Write_fun(i);                                                                                       \
    }                                                                                                         \
    ba->SetPosition(0);                                                                                       \
    for (size_t i = 0; i < vec.size(); ++i) {                                                                 \
      type v = ba->Read_fun();                                                                                \
      ASSERT(v == vec[i]);                                                                                    \
    }                                                                                                         \
    ASSERT(ba->GetReadSize() == 0);                                                                           \
    LOG_INFO(g_logger) << #Write_fun "/" #Read_fun " (" #type ") len=" << (len) << " base_len=" << (base_len) \
                       << " size=" << ba->GetSize();                                                          \
    ba->SetPosition(0);                                                                                       \
    ASSERT(ba->WriteToFile("/tmp/" #type "_" #len "-" #Read_fun ".dat"));                                     \
    wtsclwq::ByteArray::s_ptr ba2(new wtsclwq::ByteArray((base_len)*2));                                      \
    ASSERT(ba2->ReadFromFile("/tmp/" #type "_" #len "-" #Read_fun ".dat"));                                   \
    ba2->SetPosition(0);                                                                                      \
    ASSERT(ba->ToString() == ba2->ToString());                                                                \
    ASSERT(ba->GetPosition() == 0);                                                                           \
    ASSERT(ba2->GetPosition() == 0);                                                                          \
  }

  XX(int8_t, 100, WriteFint8, ReadFint8, 1);
  XX(uint8_t, 100, WriteFuint8, ReadFuint8, 1);
  XX(int16_t, 100, WriteFint16, ReadFint16, 1);
  XX(uint16_t, 100, WriteFuint16, ReadFuint16, 1);
  XX(int32_t, 100, WriteFint32, ReadFint32, 1);
  XX(uint32_t, 100, WriteFuint32, ReadFuint32, 1);
  XX(int64_t, 100, WriteFint64, ReadFint64, 1);
  XX(uint64_t, 100, WriteFuint64, ReadFuint64, 1);

  XX(int32_t, 100, WriteInt32, ReadInt32, 1);
  XX(uint32_t, 100, WriteUint32, ReadUint32, 1);
  XX(int64_t, 100, WriteInt64, ReadInt64, 1);
  XX(uint64_t, 100, WriteUint64, ReadUint64, 1);
#undef XX

/*
 * 测试用例设计：
 * 在前面的测试基础上，增加对字符串序列化/反序列化的测试
 */
#define XX(len, Write_fun, Read_fun, base_len)                                     \
  {                                                                                \
    std::string s = "qwertyuiopasdfghjklzxcvbnm";                                  \
    std::vector<std::string> vec;                                                  \
    for (int i = 0; i < (len); i++) {                                              \
      random_shuffle(s.begin(), s.end());                                          \
      vec.push_back(s);                                                            \
    }                                                                              \
    wtsclwq::ByteArray::s_ptr ba(new wtsclwq::ByteArray(base_len));                \
    for (auto &i : vec) {                                                          \
      ba->Write_fun(i);                                                            \
    }                                                                              \
    ba->SetPosition(0);                                                            \
    for (size_t i = 0; i < vec.size(); ++i) {                                      \
      std::string v = ba->Read_fun();                                              \
      ASSERT(v == vec[i]);                                                         \
    }                                                                              \
    ASSERT(ba->GetReadSize() == 0);                                                \
    LOG_INFO(g_logger) << #Write_fun                                               \
        "/" #Read_fun                                                              \
        " ("                                                                       \
        "string"                                                                   \
        ") len=" << (len)                                                          \
                       << " base_len=" << (base_len) << " size=" << ba->GetSize(); \
  }
  XX(100, WriteStringF16, ReadStringF16, 10);
  XX(100, WriteStringF32, ReadStringF32, 10);
  XX(100, WriteStringF64, ReadStringF64, 10);
  XX(100, WriteStringVint, ReadStringVint, 26);
#undef XX
}

auto main(int argc, char *argv[]) -> int {
  Test();
  return 0;
}