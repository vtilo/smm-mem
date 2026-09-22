#ifndef API_ONLY
int wmain( int argc, wchar_t **argv ) {
  if ( argc == 1 || ( argc == 2 && _wcsicmp( argv[1], L"ping" ) == 0 ) ) {
    int success = Init() && Ping();
    printf( "%s\n", success ? "pong" : "failed" );
    Close();

    return success ? 0 : 1;
  }

  printf( "Usage: mem-client.exe [ping]\n" );
  return 1;
}
#endif
