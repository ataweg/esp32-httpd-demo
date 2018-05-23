#ifndef __MIGRATION_H__
#define __MIGRATION_H__

#ifdef ESP_PLATFORM

   // --------------------------------------------------------------------------
   //
   // --------------------------------------------------------------------------

   #include <string.h>
   #include <stdio.h>
   #include <stdlib.h>
   // #include <stdarg.h>

   #include <time.h>
   #include <sys/time.h>


   // --------------------------------------------------------------------------
   //
   // --------------------------------------------------------------------------

   int ICACHE_FLASH_ATTR os_snprintf( char* buffer, size_t size, const char* format, ... );

   // --------------------------------------------------------------------------
   //  os_xxx mapping
   // --------------------------------------------------------------------------

   #ifndef os_malloc
      #define os_malloc( s ) malloc( (s ) )
   #endif

   #ifndef os_realloc
      #define os_realloc( p, s ) realloc( (p ), ( s ) )
   #endif

   #ifndef os_zalloc
      #define os_zalloc( s ) calloc( 1, ( s ) )
   #endif

   #ifndef os_free
      #define os_free( p ) free( (p ) )
   #endif

   // --------------------------------------------------------------------------
   //
   // --------------------------------------------------------------------------

   #ifndef os_strdup
      #define os_strdup( s ) strdup( s )
   #endif

   #ifndef os_memcpy
      #define os_memcpy( d, s, n ) memcpy( (d ), ( s ), ( n ) )
   #endif

   #ifndef os_memmove
      #define os_memmove( d, s, n ) memmove( (d ), ( s ), ( n ) )
   #endif

   #ifndef os_memset
      #define os_memset( s, c, n ) memset( s, c, n )
   #endif

   #ifndef os_memcmp
      #define os_memcmp( s1, s2, n ) memcmp( (s1 ), ( s2 ), ( n ) )
   #endif

   #ifndef os_strlen
      #define os_strlen( s ) strlen( s )
   #endif

   #ifndef os_strcasecmp
      #define os_strcasecmp( s1, s2 ) strcasecmp( (s1 ), ( s2 ) )
   #endif

   #ifndef os_strncasecmp
      #define os_strncasecmp( s1, s2, n ) strncasecmp( (s1 ), ( s2 ), ( n ) )
   #endif

   #ifndef os_strchr
      #define os_strchr( s, c ) strchr( (s ), ( c ) )
   #endif

   #ifndef os_strcmp
      #define os_strcmp( s1, s2 ) strcmp( (s1 ), ( s2 ) )
   #endif

   #ifndef os_strncmp
      #define os_strncmp( s1, s2, n ) strncmp( (s1 ), ( s2 ), ( n ) )
   #endif

   #ifndef os_strcpy
      #define os_strcpy( d, s )    strcpy( d, s )
   #endif

   #ifndef os_strncpy
      #define os_strncpy( d, s, n ) strncpy( (d ), ( s ), ( n ) )
   #endif

   #ifndef os_strstr
      #define os_strstr( h, n ) strstr( (h ), ( n ) )
   #endif

   // --------------------------------------------------------------------------
   //
   // --------------------------------------------------------------------------

   // #ifndef os_snprintf
   //    #define os_snprintf vsnprintf
   // #endif

   #ifndef os_printf
      #define os_printf printf
   #endif

   #ifndef os_sprintf
      #define os_sprintf sprintf
   #endif

   // --------------------------------------------------------------------------
   //
   // --------------------------------------------------------------------------
#endif   // ESP_PLATFORM

#endif // __MIGRATION_H__