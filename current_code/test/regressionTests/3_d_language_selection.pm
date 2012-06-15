package regressionTests::3_d_language_election;

use lib qw( regressionTests/lib );
use DBI;
use standardTests;
use Data::Dumper;
use strict;

sub testProfile {
  return {
    valgrind => 1,
    client => 0,
  }; 
} 

sub test {

  my %data = (
    action => 'docFilter',
    subaction => 'fullList',
    isActionRequired => 'false',
    page => '1',
    range => '3',
    sortfield => '3', # date
    sortorder => '1',
  );

  # Add a simgle document
  inTestSQL('1'); 

  # No language setting - should default to English
  o_log( "No language indication = " . Dumper( directRequest( \%data ) ) );

  # 'en' as first available language - should use English
  $data{__header_accepted-language} = 'qq,en;q=0.9,de;q=0.8,##;q=0.1';
  o_log( "header says en = " . Dumper( directRequest( \%data ) ) );

  # 'de' as first available language - should load then use German
  $data{__header_accepted-language} = 'qq,de;q=0.9,en;q=0.8,##;q=0.1';
  o_log( "header says de = " . Dumper( directRequest( \%data ) ) );

  # As above, but with unknown as cookie language - should skip cooke and use German
  $data{__header_cookie} = 'requested_lang=qq';
  o_log( "bad cookie, header says de = " . Dumper( directRequest( \%data ) ) );

  # As above, but with '##' as cookie language - load and use 'test language'
  $data{__header_cookie} = 'requested_lang=hh';
  o_log( "bad cookie, header says de = " . Dumper( directRequest( \%data ) ) );

#  # As above, but making a request where '##' is not available - should default to English for missing text
#  o_log( "bad cookie, header says de = " . Dumper( directRequest( \%data ) ) );

  return 0;
}

return 1;
