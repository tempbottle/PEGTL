// Copyright (c) 2014-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include <cassert>
#include <sstream>

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/changes.hpp>
#include <tao/pegtl/contrib/json.hpp>

#include "json_classes.hpp"
#include "json_errors.hpp"
#include "json_unescape.hpp"

namespace examples
{
   // Basic state class that stores the result of a JSON parsing run -- a single JSON object.

   struct result_state
   {
      result_state() = default;
      result_state( const result_state& ) = delete;
      result_state( result_state&& ) = delete;

      ~result_state() = default;

      void operator=( const result_state& ) = delete;
      void operator=( result_state&& ) = delete;

      std::shared_ptr< json_base > result;
   };

   // Action class for the simple cases...

   template< typename Rule >
   struct value_action
      : unescape_action< Rule >
   {
   };

   struct string_state
      : public unescape_state_base
   {
      template< typename Input, typename... States >
      explicit string_state( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      template< typename Input >
      void success( const Input& /*unused*/, result_state& result )
      {
         result.result = std::make_shared< string_json >( std::move( unescaped ) );
      }
   };

   template<>
   struct value_action< tao::TAO_PEGTL_NAMESPACE::json::null >
   {
      static void apply0( result_state& result )
      {
         result.result = std::make_shared< null_json >();
      }
   };

   template<>
   struct value_action< tao::TAO_PEGTL_NAMESPACE::json::true_ >
   {
      static void apply0( result_state& result )
      {
         result.result = std::make_shared< boolean_json >( true );
      }
   };

   template<>
   struct value_action< tao::TAO_PEGTL_NAMESPACE::json::false_ >
   {
      static void apply0( result_state& result )
      {
         result.result = std::make_shared< boolean_json >( false );
      }
   };

   template<>
   struct value_action< tao::TAO_PEGTL_NAMESPACE::json::number >
   {
      template< typename Input >
      static void apply( const Input& in, result_state& result )
      {
         std::stringstream ss;
         ss << in.string();
         long double v;
         ss >> v;  // NOTE: not quite correct for JSON but we'll use it for this simple example.
         result.result = std::make_shared< number_json >( v );
      }
   };

   // State and action classes to accumulate the data for a JSON array.

   struct array_state
      : public result_state
   {
      std::shared_ptr< array_json > array = std::make_shared< array_json >();

      template< typename Input, typename... States >
      explicit array_state( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      void push_back()
      {
         array->data.push_back( std::move( result ) );
         result.reset();
      }

      template< typename Input >
      void success( const Input& /*unused*/, result_state& in_result )
      {
         if( this->result ) {
            push_back();
         }
         in_result.result = array;
      }
   };

   template< typename Rule >
   struct array_action
      : tao::TAO_PEGTL_NAMESPACE::nothing< Rule >
   {
   };

   template<>
   struct array_action< tao::TAO_PEGTL_NAMESPACE::json::value_separator >
   {
      static void apply0( array_state& result )
      {
         result.push_back();
      }
   };

   // State and action classes to accumulate the data for a JSON object.

   struct object_state
      : public result_state
   {
      std::string unescaped;
      std::shared_ptr< object_json > object = std::make_shared< object_json >();

      template< typename Input, typename... States >
      explicit object_state( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      void insert()
      {
         object->data.insert( std::make_pair( std::move( unescaped ), std::move( result ) ) );
         unescaped.clear();
         result.reset();
      }

      template< typename Input >
      void success( const Input& /*unused*/, result_state& in_result )
      {
         if( this->result ) {
            insert();
         }
         in_result.result = object;
      }
   };

   template< typename Rule >
   struct object_action
      : unescape_action< Rule >
   {
   };

   template<>
   struct object_action< tao::TAO_PEGTL_NAMESPACE::json::value_separator >
   {
      static void apply0( object_state& result )
      {
         result.insert();
      }
   };

   // Put together a control class that changes the actions and states as required.

   // clang-format off
   template< typename Rule > struct control : errors< Rule > {};  // Inherit from json_errors.hpp.

   template<> struct control< tao::TAO_PEGTL_NAMESPACE::json::value > : tao::TAO_PEGTL_NAMESPACE::change_action< tao::TAO_PEGTL_NAMESPACE::json::value, value_action, errors > {};
   template<> struct control< tao::TAO_PEGTL_NAMESPACE::json::string::content > : tao::TAO_PEGTL_NAMESPACE::change_state< tao::TAO_PEGTL_NAMESPACE::json::string::content, string_state, errors > {};
   template<> struct control< tao::TAO_PEGTL_NAMESPACE::json::array::content > : tao::TAO_PEGTL_NAMESPACE::change_state_and_action< tao::TAO_PEGTL_NAMESPACE::json::array::content, array_state, array_action, errors > {};
   template<> struct control< tao::TAO_PEGTL_NAMESPACE::json::object::content > : tao::TAO_PEGTL_NAMESPACE::change_state_and_action< tao::TAO_PEGTL_NAMESPACE::json::object::content, object_state, object_action, errors > {};

   struct grammar : tao::TAO_PEGTL_NAMESPACE::must< tao::TAO_PEGTL_NAMESPACE::json::text, tao::TAO_PEGTL_NAMESPACE::eof > {};
   // clang-format on

}  // namespace examples

int main( int argc, char** argv )
{
   for( int i = 1; i < argc; ++i ) {
      examples::result_state result;
      tao::TAO_PEGTL_NAMESPACE::file_input in( argv[ i ] );
      tao::TAO_PEGTL_NAMESPACE::parse< examples::grammar, tao::TAO_PEGTL_NAMESPACE::nothing, examples::control >( in, result );
      assert( result.result );
      std::cout << result.result << std::endl;
   }
   return 0;
}
