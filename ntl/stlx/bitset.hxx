/**\file*********************************************************************
 *                                                                     \brief
 *  Class template bitset [23.3.5 template.bitset]
 *
 ****************************************************************************
 */

#ifndef NTL__STLX_BITSET
#define NTL__STLX_BITSET

#include "cstddef.hxx"
#include "string.hxx"
#include "stdexcept.hxx"
#include "iosfwd.hxx"

namespace std {

  /**\addtogroup  lib_containers ********* Containers library [23] ************
  *@{*/

  /**\addtogroup  lib_associative *********** Associative [23.3] *******************
  *@{*/

  template <size_t N> class bitset;

  // 23.3.5.3 bitset operators:
  template <size_t N>
  bitset<N> operator&(const bitset<N>& lhs, const bitset<N>& rhs)
  {
    return lhs &= rhs;
  }

  template <size_t N>
  bitset<N> operator|(const bitset<N>& lhs, const bitset<N>& rhs)
  {
    return lhs |= rhs;
  }

  template <size_t N>
  bitset<N> operator^(const bitset<N>& lhs, const bitset<N>& rhs)
  {
    return lhs ^= rhs;
  }

  template <class charT, class traits, size_t N>
  basic_istream<charT, traits>&
    operator>>(basic_istream<charT, traits>& is, bitset<N>& x);

  template <class charT, class traits, size_t N>
  basic_ostream<charT, traits>&
    operator<<(basic_ostream<charT, traits>& os, const bitset<N>& x);

  /// Class template bitset [23.3.5 template.bitset]
  template<size_t N>
  class bitset
  {
  public:
    // bit reference
    class reference
    {
      friend class bitset;
      reference();

      reference(bitset& b, const size_t pos)
        :bitset_(b), pos_(pos)
      {}
    public:
      ~reference(){}

      reference& operator=(bool x)
      {
        bitset_.set(pos_, x);
        return *this;
      }

      reference& operator=(const reference& rhs)
      {
        bitset_.set(pos_, rhs.bitset_.test(rhs.pos_));
        return *this;
      }

      bool operator~() const
      {
        return !bitset_.test(pos_);
      }
      
      operator bool() const
      {
        return bitset_.test(pos_);
      }

      reference& flip()
      {
        bitset_.flip(pos_);
        return *this;
      }

    private:
      bitset<N>& bitset_;
      const size_t pos_;
    };

    /// @name constructors [23.3.5.1]
    constexpr bitset()
    {
      reset();
    }

    constexpr bitset(unsigned long long val)
    {
      // compiler cannot shift more than 32
      val &= tidy<N, N >= 32>::make_tidy();
      if(N > sizeof(unsigned long long)*8)
        reset();

      // split val to i elements
      const size_t count = min(elements_count_, sizeof(unsigned long long) / sizeof(storage_type));
      for(size_t x = 0; x < count; ++x)
        storage_[x] = static_cast<storage_type>((val >> x * element_size_) & set_bits_);
    }

    template<class charT, class traits, class Allocator>
    explicit bitset(
      const basic_string<charT,traits,Allocator>& str,
      typename basic_string<charT,traits,Allocator>::size_type pos = 0,
      typename basic_string<charT,traits,Allocator>::size_type n =
      basic_string<charT,traits,Allocator>::npos
      ) throw(out_of_range, invalid_argument)
    {
      typedef basic_string<charT,traits,Allocator> string_type;
      
      // 23.3.5.1.4
      if(pos > str.size())
        __ntl_throw(out_of_range);

      // 23.3.5.1.5
      const size_t rlen = min(n, str.size()-pos);

      // 23.3.5.1.7
      if((n-pos*8) < N)
        reset();

      const size_t count = min(rlen, N);
      for(size_t i = 0, rpos = pos + count - 1; i < count; ++i, --rpos){
        const traits::char_type c = str[rpos];
        // 23.3.5.1.5.2
        if(!(c == '0' || c == '1'))
          __ntl_throw(invalid_argument);

        storage_type xval = storage_[i/element_size_];
        const size_t mod = i & element_mod_;
        xval &= ~(1 << mod);
        xval |= ((c == '1') << mod);
        storage_[i/element_size_] = xval;
      }
    }

    /// @name bitset operations [23.3.5.2]
    bitset<N>& operator&=(const bitset<N>& rhs)
    {
      for(size_t i = 0; i < elements_count_; ++i)
        storage_[i] &= ~rhs.storage_[i];
      return *this;
    }

    bitset<N>& operator|=(const bitset<N>& rhs)
    {
      for(size_t i = 0; i < elements_count_; ++i)
        storage_[i] |= rhs.storage_[i];
      return *this;
    }

    bitset<N>& operator^=(const bitset<N>& rhs)
    {
      for(size_t i = 0; i < elements_count_; ++i)
        storage_[i] ^= rhs.storage_[i];
      return *this;
    }

    bitset<N>& operator<<=(size_t pos)
    {
      if(pos > N)
        return reset();

      storage_type xval = 0, yval = 0;
      for(size_t i = 0; i < elements_count_; ++i){
        // save shifted bits
        yval = storage_[i] >> (element_size_ - pos);
        // shift
        storage_[i] <<= pos;
        // restore previous bits
        storage_[i] |= xval;
        xval = yval;
      }
      return *this;
    }

    bitset<N>& operator>>=(size_t pos)
    {
      if(pos > N)
        return reset();

      storage_type xval = 0, yval = 0;
      for(int i = elements_count_-1; i >= 0; --i){
        // save shifted bits
        yval = storage_[i] & ((1 << pos) - 1);
        // shift
        storage_[i] >>= pos;
        // restore previous bits
        storage_[i] |= xval << (element_size_ - pos);
        xval = yval;
      }
      return *this;
    }

    bitset<N>& set()
    {
      for(size_t i = 0; i < elements_count_; ++i)
        storage_[i] = set_bits_;
      return *this;
    }

    bitset<N>& set(size_t pos, bool val = true) throw(out_of_range)
    {
      check_bounds(pos);
      storage_type xval = storage_[pos / element_size_];
      const size_t mod = pos & element_mod_;
      xval &= ~(1 << mod);
      xval |= (val << mod);
      storage_[pos / element_size_] = xval;
      return *this;
    }

    bitset<N>& reset()
    {
      for(size_t i = 0; i < elements_count_; ++i)
        storage_[i] = 0;
      return *this;
    }

    bitset<N>& reset(size_t pos) throw(out_of_range)
    {
      check_bounds(pos);
      storage_type val = storage_[pos / element_size_];
      const size_t mod = pos & element_mod_;
      val &= ~(1 << mod);
      storage_[pos / element_size_] = val;
      return *this;
    }

    bitset<N> operator~() const
    {
      return bitset<N>(*this).flip();
    }

    bitset<N>& flip()
    {
      for(size_t i = 0; i < elements_count_; ++i)
        storage_[i] = ~storage_[i];
      return *this;
    }

    bitset<N>& flip(size_t pos) throw(out_of_range)
    {
      check_bounds(pos);
      storage_type val = storage_[pos / element_size_];
      const size_t mod = pos & element_mod_;
      val ^= (1 << mod);
      storage_[pos / element_size_] = val;
      return *this;
    }

    constexpr bool operator[](size_t pos) const throw()
    { return test(pos); }

    reference operator[](size_t pos) throw()
    {
      return reference(*this, pos);
    }

    unsigned long to_ulong() const throw (overflow_error)
    {
      return to_T<unsigned long>();
    }

    unsigned long long to_ullong() const throw (overflow_error)
    {
      return to_T<unsigned long long>();
    }

    template <class charT, class traits, class Allocator>
    basic_string<charT, traits, Allocator> to_string() const
    {
      return to_stringT<charT, traits, Allocator>();
    }
    template <class charT, class traits>
    basic_string<charT, traits, allocator<charT> > to_string() const
    {
      return to_stringT<charT, traits, allocator<charT> >();
    }
    template <class charT>
    basic_string<charT, char_traits<charT>, allocator<charT> > to_string() const
    {
      return to_stringT<charT, char_traits<charT>, allocator<charT> >();
    }
    basic_string<char, char_traits<char>, allocator<char> > to_string() const
    {
      return to_stringT<char, char_traits<char>, allocator<char> >();
    }

    size_t count() const
    {
      static const char table[] = "\0\1\1\2\1\2\2\3\1\2\2\3\2\3\3\4";
      size_t count_ = 0;
      for(int i = elements_count_-1; i >= 0; --i)
        for(storage_type j = storage_[i] & digitd_mod_; j; j >>= 4)
          count_ += table[j & 0xF];
      return count_;
    }

    constexpr size_t size() const { return N; }

    bool operator==(const bitset<N>& rhs) const
    {
      return !memcmp(storage_, rhs.storage_, elements_count_*sizeof(storage_type));
    }

    bool operator!=(const bitset<N>& rhs) const
    {
      return !(*this==rhs);
    }

    bool test(size_t pos) const throw(out_of_range)
    {
      check_bounds(pos);
      const storage_type val = storage_[pos / element_size_];
      return (val & (1 << (pos & element_mod_))) != 0;
    }

    bool all() const { return count() == size(); }
    bool any() const { return count() != 0; }
    bool none() const{ return count() == 0; }

    bitset<N> operator<<(size_t pos) const
    {
      return bitset<N>(*this) <<= pos;
    }

    bitset<N> operator>>(size_t pos) const
    {
      return bitset<N>(*this) >>= pos;
    }

  private:
    void check_bounds(const size_t pos) const throw(out_of_range)
    {
      if(pos >= N)
        __ntl_throw(out_of_range);
    }

    template<typename T>
    T to_T() const throw (overflow_error)
    {
      if(sizeof(T)*8 < N){
        // check that all upper bits are zero
        for(size_t pos = elements_count_-1; pos >= sizeof(T) / sizeof(storage_type); --pos){
          if(storage_[pos] != 0)
            __ntl_throw(overflow_error);
        }
      }
      T val = 0;
      for(size_t pos = 0; pos < min(sizeof(T) / sizeof(storage_type), elements_count_); ++pos)
        val |= (T)storage_[pos] << (pos*element_size_);
      return val;
    }

    template <class charT, class traits, class Allocator>
    basic_string<charT, traits, Allocator> to_stringT() const
    {
      basic_string<charT, traits, Allocator> str;
      str.resize(N);
      for(size_t word = 0; word < elements_count_; ++word){
        const size_t rpos = N - word*element_size_ - 1;
        const storage_type xval = storage_[word];
        const size_t count = min(rpos+1, (size_t)element_size_);
        for(size_t bit = 0; bit < count; ++bit){
          str[rpos-bit] = static_cast<charT>('0' + ((xval & (1 << bit)) != 0));
        }
      }
      return str;
    }

  private:
    typedef uintptr_t storage_type; // native platform type

    enum { digits = N };
    enum { element_size_ = sizeof(storage_type) * 8 };

    static const size_t element_mod_ = element_size_ - 1;
    static const size_t digitd_mod_ = (1 << (N & (element_size_-1))) - 1;
    static const size_t elements_count_ = N / element_size_ + ((N & (element_size_-1)) ? 1 : 0);
    static const storage_type set_bits_ = static_cast<storage_type>(-1);

    template<size_t size, bool large> struct tidy 
    { 
      static const uint64_t make_tidy()
      {
        return (1 << size) - 1;
      }
    };
    template<size_t size> struct tidy<size, true>
    {
      static const uint64_t make_tidy()
      {
        uint64_t value = 1;
        value <<= size;
        return --value;
      }
    };

    storage_type storage_[elements_count_];
  };

  ///@}
  /**@} lib_associative */
  /**@} lib_containers */

} // namespace std

#endif // NTL__STLX_BITSET
