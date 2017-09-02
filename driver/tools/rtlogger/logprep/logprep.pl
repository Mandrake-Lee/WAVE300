#!/usr/bin/perl -w
#
# Metalink logger preprocessor script
#
# Usage: perl logprep.pl <Origin ID> <Origin Name> <GID list header file> <macro database header file> <macro database source file> <scd file name>
#
# $Id: logprep.pl 12441 2012-01-12 14:40:48Z laptijev $
#

use English;

my $debug = 0;

# These falues are for maximum length checks
# and must be synchronized with corresponding
# definitions in logdefs.h
my $MAX_LID_VALUE   = (1 << 14) - 1;
my $MAX_FID_VALUE   = (1 <<  5) - 1;
my $MAX_DSIZE_VALUE = (1 << 11) - 1;
my $MAX_GID_VALUE   = (1 <<  7) - 1;
my $MAX_OID_VALUE   = (1 <<  3) - 1;

# This is a special OID value for 
# components that does not have remote logging support
my $NO_OID_VALUE    = 255;

# When it comes to multiline macros that use __LINE__
# macro inside there is a difference in preprocessors behaviour
# Some of them (like one used in GCC) substitute __LINE__ with number
# of the last line occupied by macro, others, like one used in Green Hils MULTI
# substitute it with number of the first line occupied by macro
# This variable changes behaviour of the script accordingly.
my $multiline_macro_binding_first_line = 0;

my $HeaderData="";
my $SourceData="";
my @GIDListOrig=();
my @GIDNamesList=();
my @StringsList=();

my $curr_group_name;
my $curr_file_id;

my $usage_text = <<USAGE_END;
The command line is incorrect.
Usage:
    perl logprep.pl <Compiler> <Origin ID> <Origin Name> <GID list header file> <macro database header file> <macro database source file> <scd file name>
    Compilers supported:
        1. gcc
        2. mutli

USAGE_END

($#ARGV + 1) == 7 or die($usage_text);

(my $TargetCompiler, my $OriginID, my $OriginName, my $GIDListFile, my $MacroHeaderFile, my $MacroSourceFile, my $SCDFile) = @ARGV;

if(($OriginID > $MAX_OID_VALUE) && ($OriginID != $NO_OID_VALUE))
{
  print STDERR "Origin ID is too big ($OriginID > $MAX_OID_VALUE)\n";
  exit 1;
}

if($TargetCompiler eq "gcc")
{
    $multiline_macro_binding_first_line = 0;
}
elsif($TargetCompiler eq "multi")
{
    $multiline_macro_binding_first_line = 1;
}
else
{
    die($usage_text);
}

sub dbg_print($)
{
    printf(STDERR shift) if($debug);
}

sub generate_param_lists
{
  my $macro_suffix = shift;

  %type_by_format_char = (S => "const char *",
                          D => "int32",
                          C => "int8",
                          P => "const void *",
                          H => "int64",
                          Y => "const void *",
                          K => "const void *");

  %packer_by_format_char = (S => "LOGPKT_PUT_STRING",
                            D => "LOGPKT_PUT_INT32",
                            C => "LOGPKT_PUT_INT8",
                            P => "LOGPKT_PUT_PTR",
                            H => "LOGPKT_PUT_INT64",
                            Y => "LOGPKT_PUT_MACADDR",
                            K => "LOGPKT_PUT_IP6ADDR");

  %size_by_format_char = (S => "LOGPKT_STRING_SIZE",
                          D => "LOGPKT_SCALAR_SIZE",
                          C => "LOGPKT_SCALAR_SIZE",
                          P => "LOGPKT_SCALAR_SIZE",
                          H => "LOGPKT_SCALAR_SIZE",
                          Y => "LOGPKT_MACADDR_SIZE",
                          K => "LOGPKT_IP6ADDR_SIZE");

  $macro_suffix =~ s/V//g;

  my $macro_params_list = $macro_suffix;
  $macro_params_list =~ s/(.)/", ".lc($1).(pos($macro_params_list) + 1)/ge;

  my $func_params_list = $macro_suffix;
  $func_params_list =~ s/(.)/", ".$type_by_format_char{$1}." ".lc($1).(pos($func_params_list) + 1)/ge;

  my $pass_params_list = $macro_suffix;
  $pass_params_list =~ s/(.)/", (".$type_by_format_char{$1}.") (".lc($1).(pos($pass_params_list) + 1).")"/ge;

  my $total_params_size = $macro_suffix;
  $total_params_size =~ s/(.)/"\n                       + ".$size_by_format_char{$1}."(".lc($1).(pos($total_params_size) + 1).")"/ge;

  my $pack_params_code = $macro_suffix;
  $pack_params_code =~ s/(.)/"      ".$packer_by_format_char{$1}."(".lc($1).(pos($pack_params_code) + 1).");\n"/ge;

  my $str_length_calc = $macro_suffix;
  $str_length_calc =~ s/[^S]/@/g;
  $str_length_calc =~ s/(S)/"    size_t ".lc($1).(pos($str_length_calc) + 1)."len__ = strlen(".lc($1).(pos($str_length_calc) + 1).") + 1;\n"/ge;
  $str_length_calc =~ s/@//g;

  return ($macro_params_list, $func_params_list, $pass_params_list, $total_params_size,
          $pack_params_code, $str_length_calc);
}

sub generate_code
{
  my $macro_name = shift;
  my $log_level = shift;
  my $macro_suffix = shift;

  (my $macro_params_list, my $func_params_list, my $pass_params_list, my $total_params_size,
   my $pack_params_code, my $str_length_calc) =
       generate_param_lists($macro_suffix);

  my $console_printout;

  if($log_level eq "RTLOG_ERROR_DLEVEL") {
    $console_printout = "CERROR(fname, lid, fmt$pass_params_list);";
  } elsif($log_level eq "RTLOG_WARNING_DLEVEL") {
    $console_printout = "CWARNING(fname, lid, fmt$pass_params_list);";
  } else {
    $console_printout = "CLOG(fname, lid, $log_level, fmt$pass_params_list);";
  }

  my $header = <<END_OF_HEADER;
#if (RTLOG_MAX_DLEVEL < $log_level)
#define $macro_name\_$macro_suffix(fmt$macro_params_list)
#else
__MTLK_FLOG void
__$macro_name\_$macro_suffix\_$OriginID(const char *fname, uint8 gid, uint8 fid, uint16 lid, const char *fmt$func_params_list);

#define $macro_name\_$macro_suffix(fmt$macro_params_list) \\
  __$macro_name\_$macro_suffix\_$OriginID(LOG_CONSOLE_TEXT_INFO, LOG_LOCAL_GID, LOG_LOCAL_FID, __LINE__, \\
               (fmt)$pass_params_list)
#endif /* RTLOG_MAX_DLEVEL < $log_level */
END_OF_HEADER

  my $body = <<END_OF_BODY;
#if (RTLOG_MAX_DLEVEL >= $log_level)
__MTLK_FLOG void
__$macro_name\_$macro_suffix\_$OriginID(const char *fname, uint8 gid, uint8 fid, uint16 lid, const char *fmt$func_params_list)
{
#if (RTLOG_FLAGS & (RTLF_REMOTE_ENABLED | RTLF_CONSOLE_ENABLED))
  int flags__ = mtlk_log_get_flags($log_level, $OriginID, gid);
#endif
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  if ((flags__ & LOG_TARGET_REMOTE) != 0) {

$str_length_calc
    size_t datalen__ = 0$total_params_size;
    uint8 *pdata__;
    mtlk_log_event_t log_event__;
    mtlk_log_buf_entry_t *pbuf__ = mtlk_log_new_pkt_reserve(LOGPKT_EVENT_HDR_SIZE + datalen__, &pdata__);
    if (pbuf__ != NULL) {
      uint8 *p__ = pdata__;

      MTLK_ASSERT(pdata__ != NULL);
      MTLK_ASSERT(datalen__ <= $MAX_DSIZE_VALUE);

      log_event__.timestamp = mtlk_log_get_timestamp();
      log_event__.info = LOG_MAKE_INFO(0, $OriginID, gid);
      log_event__.info_ex = LOG_MAKE_INFO_EX(fid, lid, datalen__, 0);

      /* We do not copy the whole structure to avoid issues with  */
      /* incorrect packing. Client side assumes this structure    */
      /* is 1-byte packed, but some compilers have issues with    */
      /* creation of such structures.                             */
      /* WARNING: Because of this type of usage order of fields   */
      /* in the sctructure is important and must be preserved.    */
      logpkt_memcpy(p__, &log_event__, LOGPKT_EVENT_HDR_SIZE);

    p__ += LOGPKT_EVENT_HDR_SIZE;

$pack_params_code
      MTLK_ASSERT(p__ == pdata__ + datalen__ + LOGPKT_EVENT_HDR_SIZE);
      mtlk_log_new_pkt_release(pbuf__);
    }
  }
#endif
#if (RTLOG_FLAGS & RTLF_CONSOLE_ENABLED)
  if ((flags__ & LOG_TARGET_CONSOLE) != 0) {
    $console_printout
  }
#endif
}
#endif /* RTLOG_MAX_DLEVEL >= $log_level */
END_OF_BODY

  return ($header, $body);
}

sub get_macro_suffix_string
{
  my $string = shift;
  my $packed_format = "";

  dbg_print "The format string is ===========================\n$string\n=============================\n";

  $string =~ s/
                (?: (?<!%)(?:%%)*%(?!%) )    #Match odd number of percent sign repetitions only
                (?:
                  .*?                        #flags, width, precision
                  (l*)                       #length
                  ([cduixXspBYK])            #Format letter
                )
              /
                my $format_symbol;
                my $length=$1;
                my $format_letter=$2;

                if($format_letter =~ \/[diuxX]\/)     { $format_symbol = (length($length) < 2) ? "D" : "H"; }
                elsif($format_letter =~ \/[B]\/)      { $format_symbol = "D"; }
                else                                  { $format_symbol = uc($format_letter); }

                dbg_print "===length: $length===format_letter: $format_letter===format_symbol: $format_symbol===\n";
                $packed_format = $packed_format.$format_symbol;
              /xeg;

  $packed_format = "V" if(not $packed_format);
  dbg_print "The packed format string is $packed_format ===========================\n";

  return $packed_format;
}

sub is_curr_file_id
{
  my $group_name = shift;
  my $file_id = shift;

  return (($group_name eq $curr_group_name) and ($file_id == $curr_file_id));
}

sub clear_current_file_strings
{
  return if not defined $curr_group_name or not defined $curr_file_id;

  my @NewStringList;

  foreach  $StringRef (@StringsList)
  {
    (my $line_number, my $group_name, my $file_id, my $format_string) = @$StringRef;
    push @NewStringList, [ ($line_number, $group_name, $file_id, $format_string) ] if not is_curr_file_id($group_name, $file_id);
  }

  @StringsList = @NewStringList;
}

sub process_group_definition
{
  my $group_name = shift;

  push(@GIDNamesList, $group_name);
  $curr_group_name = $group_name;
  clear_current_file_strings();

  return $group_name;
}

sub process_file_definition
{
  $curr_file_id = shift;

  if($curr_file_id > $MAX_FID_VALUE)
  {
    print STDERR "File ID is too big ($curr_file_id > $MAX_FID_VALUE)\n";
    exit 1;
  }

  clear_current_file_strings();

  return $curr_file_id;
}

sub process_print_command
{
  my $line = shift;
  my $line_number = shift;
  my $macro = shift;
  my $log_level = shift;
  my $text_before_format_string = shift;
  my $format_string = shift;
  my $text_after_format_string = shift;

  $log_level = "RTLOG_ERROR_DLEVEL" if $macro =~ /^ELOG.*/;
  $log_level = "RTLOG_WARNING_DLEVEL" if $macro =~ /^WLOG.*/;

  if (not defined $log_level)
  {
    #If log level not specified then this is not a printout we recognize
    #do nothing and keep the code as is.
    return $macro.$text_before_format_string.$format_string.$text_after_format_string;
  }

  dbg_print "The line is ===========================\n$line\n=============================\n";
  dbg_print "=============================\n";
  dbg_print "macro is                        ====".$macro."====\n";
  dbg_print "log level is                    ====".$log_level."====\n";
  dbg_print "text before format string is    ====".$text_before_format_string."====\n";
  dbg_print "format string is                ====".$format_string."====\n";
  dbg_print "text after format string is     ====".$text_after_format_string."====\n";
  dbg_print "=============================\n";

  my $macro_suffix = get_macro_suffix_string($format_string);

  #Eliminate escaped endlines and merge chunks of
  #the format string and put it into the strings database
  my $SCDFormatString = $format_string;
  $SCDFormatString =~ s/\\\n//gm;
  $SCDFormatString =~ s/\"[\s\n]*\"//gm;

  if (not defined $curr_group_name)
  {
    print STDERR "No group name definition before printout at line $line_number\n";
    exit 1;
  }

  if (not defined $curr_file_id)
  {
    print STDERR "No file ID definition before printout at line $line_number\n";
    exit 1;
  }

  if($line_number > $MAX_LID_VALUE)
  {
    print STDERR "Line ID is too big ($line_number > $MAX_LID_VALUE)\n";
    exit 1;
  }

  push @StringsList, [ ($line_number, $curr_group_name, $curr_file_id, $SCDFormatString) ];

  #Generate the macro and put it to code database
  (my $header, my $body) = generate_code($macro, $log_level, $macro_suffix);
  if( -1 == index($HeaderData, " $macro\_$macro_suffix(") )
  {
    $HeaderData .= $header."\n\n";
    $SourceData .= $body."\n\n";
  }

  return $macro."_".$macro_suffix.$text_before_format_string.$format_string.$text_after_format_string;
}

#Read GID list and build number to name conversion hash
my %GroupIDToName = ();
if(-e $GIDListFile)
{
  open my $fh, '<', $GIDListFile or die "error opening $GIDListFile for reading: $!";
  local $INPUT_RECORD_SEPARATOR="\n";
  while (<$fh>)
  {
      die "Incorrect GID list entry $_" if(not s/^\#define\s+(\w+)\s+(\w+).*$/push(@GIDNamesList, $1) and ($GroupIDToName{$2} = $1)/eg);
  }
  @GIDListOrig = @GIDNamesList;
}

#Read headers database
if(-e $MacroHeaderFile)
{
  open my $fh, '<', $MacroHeaderFile or die "error opening $MacroHeaderFile for reading: $!";
  $HeaderData = do { local $/; <$fh> };
}

#Read source database
if(-e $MacroSourceFile)
{
  open my $fh, '<', $MacroSourceFile or die "error opening $MacroSourceFile for reading: $!";
  $SourceData = do { local $/; <$fh> };
}

#Read string list
if(-e $SCDFile)
{
  open my $fh, '<', $SCDFile or die "error opening $SCDFile for reading: $!";
  while (<$fh>)
  {
    s/^S\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s+(.*)$
     /push(@StringsList, [ ($4, $GroupIDToName{$2}, $3, $5) ])/egx
  }
}

$INPUT_RECORD_SEPARATOR=";";
my $MacroNameRe = '((?<!\w)[IWE]LOG([0-9]){0,1})(?: _[A-Z]+){0,1}';
my $CurrentLineNumber = 1;

while (<STDIN>)
{
    my $line=$_;
    my $orig_line = $line;

    #If chunk contains printout - get number of lines before printout
    #This number will be used to determine line number of the printout macro
    my $LinesBeforePrintout = 0;
    if($line =~ /.*$MacroNameRe(.|\n)*/)
    {
      if($multiline_macro_binding_first_line)
      {
          $line =~ s/$MacroNameRe(.|\n)*//mg;
      }
      $LinesBeforePrintout = $line =~ s/\n/\n/mg;
      $line = $orig_line;
    }

    #Find group IDS
    $line =~ s/(LOG_LOCAL_GID\s+)(\w+)/$1.process_group_definition($2)/xemg;
    $line =~ s/(LOG_LOCAL_FID\s+)(\w+)/$1.process_file_definition($2)/xemg;

    #Process printouts
    $line =~ s/
               $MacroNameRe                                    #Macro name
               ((?: [^"]|\n)*")                                #Text between macro name and beginning of the format string
               ((?: .|\n)*?)                                   #Format string
               ((?<!\\)"\s*[\,\)](?: .|\n)*)                   #Text after format string
              /process_print_command($orig_line, $CurrentLineNumber + $LinesBeforePrintout, $1, $2, $3, $4, $5)/xemg;

    $CurrentLineNumber += $line =~ s/\n/\n/mg;

    print $line;
}

open my $fhSCD, '>', $SCDFile or die "error opening $SCDFile for writing: $!";
print $fhSCD "O $OriginID $OriginName\n";

#Flush GID list into both C header and SCD file
my $i=1;
my @GIDListNew = ();
my %seen = ();

foreach $GID (@GIDNamesList)
{
  unless ($seen{$GID}++)
  {
    push(@GIDListNew, $GID);

    if($i > $MAX_GID_VALUE)
    {
      print STDERR "Group ID is too big for $GID ($i > $MAX_GID_VALUE)\n";
      exit 1;
    }

    print $fhSCD "G $OriginID ".$i++." $GID\n";
  }
}

#If new GIDs were added we have to rebuild the GID list file
if($#GIDListNew != $#GIDListOrig)
{
  $i=1;
  open my $fh, '>', $GIDListFile or die "error opening $GIDListFile for writing: $!";

  foreach $GID (@GIDListNew)
  {
    print $fh "\#define $GID\t".$i++."\n";
  }

  close $fh;
}

#Build group name to ID conversion hash
my %GroupNameToID = ();
$i=1;
foreach $GID (@GIDListNew)
{
  $GroupNameToID{$GID} = $i++;
}

foreach  $StringRef (@StringsList)
{
  (my $line_number, my $group_id, my $file_id, my $format_string) = @$StringRef;
  print $fhSCD "S $OriginID ".$GroupNameToID{$group_id}." $file_id "."$line_number $format_string\n";
}

close $fhSCD;

#Flush headers database
open $fh, '>', $MacroHeaderFile or die "error opening $MacroHeaderFile for writing: $!";
print $fh $HeaderData;
close $fh;

#Flush source database
open $fh, '>', $MacroSourceFile or die "error opening $MacroSourceFile for writing: $!";
print $fh $SourceData;
close $fh;

exit 0;
