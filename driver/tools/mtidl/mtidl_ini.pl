#!/usr/bin/perl -w
#
# MTIDL to .ini compiler
#
# Usage: perl mtidl_ini.pl ...
#
# $Id: mtidl_ini.pl 12036 2011-11-27 18:08:05Z fleytman $
#

use English;
my $debug=0;

sub dbg_print($)
{
  printf(STDERR shift) if($debug);
}

sub error_exit($)
{
  printf(STDERR "\n\nERROR: ".(shift)."\n\n");
  exit(123);
}

sub add_definition_to_dictionary
{
  my $dictionary_ref = shift;
  my $key = shift;
  my $value = shift;

  ${$dictionary_ref}{$key} = $value;
}

my $word_re = '[A-Za-z_0-9][A-Za-z_0-9]*';
my $int_re = '[\-\+]{0,1}(?: 0x){0,1}[0-9A-Fa-f][0-9A-Fa-f]*';

my $mtidl_arg_tuple_re = '\((.*?)\)';
my $mtidl_enum_arg_tuple_re = '\(\s*('.$word_re.')\s*,\s*('.$int_re.')\s*,\s*\"(.*?)\"\s*\)';
my $mtidl_fract_arg_tuple_re = '\(\s*('.$word_re.')\s*,\s*('.$int_re.')\s*,\s*\"(.*?)\"\s*\)';
my $mtidl_fract_array_arg_tuple_re = '\(\s*('.$word_re.')\s*,\s*('.$int_re.')\s*,\s*('.$int_re.')\s*,\s*\"(.*?)\"\s*\)';

my $mtidl_item_id_re        = 'MTIDL_ID'.$mtidl_arg_tuple_re;
my $mtidl_item_type_re      = 'MTIDL_TYPE'.$mtidl_arg_tuple_re;
my $mtidl_item_level_re     = 'MTIDL_LEVEL'.$mtidl_arg_tuple_re;
my $mtidl_item_source_re    = 'MTIDL_SOURCE'.$mtidl_arg_tuple_re;
my $mtidl_item_constant_re  = 'MTIDL_CONST'.$mtidl_arg_tuple_re;
my $mtidl_enum_entry_re     = 'MTIDL_ENUM_ENTRY'.$mtidl_enum_arg_tuple_re;
my $mtidl_bitfield_entry_re = 'MTIDL_BITFIELD_ENTRY'.$mtidl_enum_arg_tuple_re;

my $mtidl_val_arg_tuple_re       = '\((?: \s*'.$word_re.'\s*,\s*)*\"(.*?)\"\s*\)';
my $mtidl_item_val_arg_tuple_re  = '\(\s*('.$word_re.')\s*,(?: \s*'.$word_re.'\s*,\s*)*\"(.*?)\"\s*\)';
my $mtidl_enum_val_arg_tuple_re       = '\(\s*('.$word_re.')\s*,\s*('.$word_re.')\s*,\s*\"(.*?)\"\s*\)';

my $mtidl_val_long_re = 'MTIDL_LONGVAL'.$mtidl_val_arg_tuple_re;
my $mtidl_val_slong_re = 'MTIDL_SLONGVAL'.$mtidl_val_arg_tuple_re;
my $mtidl_val_huge_re = 'MTIDL_HUGEVAL'.$mtidl_val_arg_tuple_re;
my $mtidl_val_shuge_re = 'MTIDL_SHUGEVAL'.$mtidl_val_arg_tuple_re;
my $mtidl_val_time_re = 'MTIDL_TIMESTAMP'.$mtidl_val_arg_tuple_re;
my $mtidl_val_flag_re = 'MTIDL_FLAG'.$mtidl_val_arg_tuple_re;
my $mtidl_val_item_re = 'MTIDL_ITEM'.$mtidl_item_val_arg_tuple_re;
my $mtidl_val_enum_re = 'MTIDL_ENUM'.$mtidl_enum_val_arg_tuple_re;
my $mtidl_val_bitfield_re = 'MTIDL_BITFIELD'.$mtidl_enum_val_arg_tuple_re;
my $mtidl_val_macaddr_re = 'MTIDL_MACADDR'.$mtidl_val_arg_tuple_re;
my $mtidl_val_long_fract_re = 'MTIDL_LONGFRACT'.$mtidl_fract_arg_tuple_re;
my $mtidl_val_slong_fract_re = 'MTIDL_SLONGFRACT'.$mtidl_fract_arg_tuple_re;
my $mtidl_val_long_fract_array_re = 'MTIDL_LONGFRACT_ARRAY'.$mtidl_fract_array_arg_tuple_re;
my $mtidl_val_slong_fract_array_re = 'MTIDL_SLONGFRACT_ARRAY'.$mtidl_fract_array_arg_tuple_re;

my $mtidl_array_arg_tuple_re = '\((?: \s*'.$word_re.'\s*,\s*)*(.*?)\s*,\s*\"(.*?)\"\s*\)';
my $mtidl_array_item_arg_tuple_re  = '\(\s*('.$word_re.')\s*,(?: \s*'.$word_re.'\s*,\s*)*(.*?)\s*,\s*\"(.*?)\"\s*\)';
my $mtidl_enum_array_arg_tuple_re = '\(\s*('.$word_re.')\s*,\s*('.$word_re.')\s*,\s*('.$word_re.')\s*,\s*\"(.*?)\"\s*\)';

my $mtidl_array_long_re = 'MTIDL_LONGVAL_ARRAY'.$mtidl_array_arg_tuple_re;
my $mtidl_array_huge_re = 'MTIDL_HUGEVAL_ARRAY'.$mtidl_array_arg_tuple_re;
my $mtidl_array_time_re = 'MTIDL_TIMESTAMP_ARRAY'.$mtidl_array_arg_tuple_re;
my $mtidl_array_flag_re = 'MTIDL_FLAG_ARRAY'.$mtidl_array_arg_tuple_re;
my $mtidl_array_item_re = 'MTIDL_ITEM_ARRAY'.$mtidl_array_item_arg_tuple_re;
my $mtidl_array_enum_re = 'MTIDL_ENUM_ARRAY'.$mtidl_enum_array_arg_tuple_re;
my $mtidl_array_bitfield_re = 'MTIDL_BITFIELD_ARRAY'.$mtidl_enum_array_arg_tuple_re;
my $mtidl_array_macaddr_re = 'MTIDL_MACADDR_ARRAY'.$mtidl_array_arg_tuple_re;

sub process_definition
{
  my $args_string = shift;
  my $dictionary_ref = shift;

  dbg_print("process_definition($args_string)\n");

  my $args_list_re = '^\s*('.$word_re.')\s*\,\s*(\d\d*)\s*$';

  if($args_string !~ s/$args_list_re/add_definition_to_dictionary($dictionary_ref, $1, $2)/xemg)
  {
    error_exit("Failed to parse definition arguments \"$args_string\"");
  }
}

sub fill_item_properties
{
  my $dictionary_ref = shift;
  ${$dictionary_ref}{"friendly_name"} = shift;
  ${$dictionary_ref}{"type"} = shift;
  ${$dictionary_ref}{"level"} = shift;
  ${$dictionary_ref}{"source"} = shift;
  ${$dictionary_ref}{"id"} = shift;

  ${$dictionary_ref}{"description"} = "\"".(shift)."\"";
  ${$dictionary_ref}{"binary_type"} = shift;

  dbg_print("\nfill_item_properties(".${$dictionary_ref}{"friendly_name"}.","
                                     .${$dictionary_ref}{"type"}.","
                                     .${$dictionary_ref}{"level"}.","
                                     .${$dictionary_ref}{"source"}.","
                                     .${$dictionary_ref}{"id"}.","
                                     .${$dictionary_ref}{"description"}.","
                                     .${$dictionary_ref}{"binary_type"}.")\n\n");

  return "";
}

sub process_item_field
{
  my $dictionary_ref = shift;
  my $num_elements = shift;
  my $element_size = shift;
  my $fract_size = shift;
  my $description = shift;
  my $binary_type = shift;
  my $binary_subtype = shift;

  if((($binary_type eq "fract") || ($binary_type eq "sfract")) && ($fract_size == 0))
  {
    error_exit("fract_size should not be equal zero for \"$description\"");
  }

  my %field_properties;
  $field_properties{"num_elements"} = $num_elements;
  $field_properties{"fract_size"} = $fract_size;
  $field_properties{"element_size"} = lookup_constant($element_size);
  $field_properties{"description"} = "\"".$description."\"";
  $field_properties{"binary_type"} = $binary_type;
  $field_properties{"binary_subtype"} = $binary_subtype;

  push(@{${$dictionary_ref}{"fields"}}, \%field_properties);

  return "";
}

my %mtidl_ids;
my %mtidl_types;
my %mtidl_levels;
my %mtidl_sources;
my %mtidl_constants;

sub lookup_identifier
{
  my $hash_ref = shift;
  my $key = shift;
  my $entity = shift;

  error_exit("Failed to parse $entity \"$key\"") unless defined ${$hash_ref}{$key};
  return ${$hash_ref}{$key};

}

sub lookup_type { return lookup_identifier(\%mtidl_types, shift, "type"); }
sub lookup_level { return lookup_identifier(\%mtidl_levels, shift, "level"); }
sub lookup_source { return lookup_identifier(\%mtidl_sources, shift, "source"); }
sub lookup_id { return lookup_identifier(\%mtidl_ids, shift, "id"); }
sub lookup_constant 
{
  my $value=shift;

  return $value if($value =~ /$int_re/);
  return lookup_identifier(\%mtidl_constants, $value, "constant"); 
}

sub process_item_line
{
  my $items_dictionary = shift;
  my $line = shift;
  my $orig_line = $line;

  # Skip empty lines
  return "" if($line =~ /^[\s\n\r]*$/);

  dbg_print("\tprocess_item_line($line)\n");

  $line =~ s/$mtidl_val_long_re/process_item_field($items_dictionary, 1, 4, "", $1, "long", "")/xemg;
  $line =~ s/$mtidl_val_slong_re/process_item_field($items_dictionary, 1, 4, "", $1, "slong", "")/xemg;
  $line =~ s/$mtidl_val_macaddr_re/process_item_field($items_dictionary, 1, 8, "", $1, "macaddr", "")/xemg;
  $line =~ s/$mtidl_val_huge_re/process_item_field($items_dictionary, 1, 8, "", $1, "huge", "")/xemg;
  $line =~ s/$mtidl_val_shuge_re/process_item_field($items_dictionary, 1, 8, "", $1, "shuge", "")/xemg;
  $line =~ s/$mtidl_val_time_re/process_item_field($items_dictionary, 1, 4, "", $1, "time", "")/xemg;
  $line =~ s/$mtidl_val_flag_re/process_item_field($items_dictionary, 1, 4, "", $1, "flag", "")/xemg;
  $line =~ s/$mtidl_val_item_re/process_item_field($items_dictionary, 1, 0, "", $2, $1, "")/xemg;
  $line =~ s/$mtidl_val_bitfield_re/process_item_field($items_dictionary, 1, 4, "", $3, "bitfield", $2)/xemg;
  $line =~ s/$mtidl_val_enum_re/process_item_field($items_dictionary, 1, 4, "", $3, "enum", $2)/xemg;
  $line =~ s/$mtidl_val_long_fract_re/process_item_field($items_dictionary, 1, 4, $2, $3, "fract", "")/xemg;
  $line =~ s/$mtidl_val_slong_fract_re/process_item_field($items_dictionary, 1, 4, $2, $3, "sfract", "")/xemg;

  $line =~ s/$mtidl_array_long_re/process_item_field($items_dictionary, $1, 4, "", $2, "long", "")/xemg;
  $line =~ s/$mtidl_array_macaddr_re/process_item_field($items_dictionary, $1, 8, "", $2, "macaddr", "")/xemg;
  $line =~ s/$mtidl_array_huge_re/process_item_field($items_dictionary, $1, 8, "", $2, "huge", "")/xemg;
  $line =~ s/$mtidl_array_time_re/process_item_field($items_dictionary, $1, 4, "", $2, "time", "")/xemg;
  $line =~ s/$mtidl_array_flag_re/process_item_field($items_dictionary, $1, 4, "", $2, "flag", "")/xemg;
  $line =~ s/$mtidl_array_item_re/process_item_field($items_dictionary, $2, 0, "", $3, $1, "")/xemg;
  $line =~ s/$mtidl_array_bitfield_re/process_item_field($items_dictionary, $2, 4, "", $4, "bitfield", $3)/xemg;
  $line =~ s/$mtidl_array_enum_re/process_item_field($items_dictionary, $2, 4, "", $4, "enum", $3)/xemg;
  $line =~ s/$mtidl_val_long_fract_array_re/process_item_field($items_dictionary, $2, 4, $3, $4, "fract", "")/xemg;
  $line =~ s/$mtidl_val_slong_fract_array_re/process_item_field($items_dictionary, $2, 4, $3, $4, "sfract", "")/xemg;

  if($line eq $orig_line)
  {
    # Drop carriage returns for more readable output
    $orig_line =~ s/[\n\r]//mg;
    error_exit("Unprocessed line found: \"$orig_line\"");
  }
}

sub process_item
{
  my $items_array_ref=shift;
  my $text = shift;
  my $binary_type=shift;

  dbg_print("\nprocess_item($text, $binary_type)\n\n");

  my %item_dictionary;

  $text =~ s/
             ^\s*\(\s*
             ($word_re)                                     #Friendly name
             \s*,\s*
             ($word_re)                                     #Type
             \s*,\s*
             ($word_re)                                     #Level
             \s*,\s*
             ($word_re)                                     #Source
             \s*,\s*
             ($word_re)                                     #ID
             \s*,\s*
             \"(.*?)\"\s*\)                                 #Description
            /fill_item_properties(\%item_dictionary,
                                  $1,
                                  lookup_type($2),
                                  lookup_level($3),
                                  lookup_source($4),
                                  lookup_id($5),
                                  $6,
                                  $binary_type)/xemg;

  $text =~ s/^(.*)$/process_item_line(\%item_dictionary, $1)/xemg;

  push(@{$items_array_ref}, \%item_dictionary);

  return "";
}

sub process_enum_entry
{
  my $dictionary_ref = shift;
  my $id = shift;
  my $value = shift;
  my $name = shift;

  my %entry_properties;
  $entry_properties{"value"} = $value;
  $entry_properties{"name"} = "\"".$name."\"";

  push(@{${$dictionary_ref}{"fields"}}, \%entry_properties);

  return "";
}

sub process_enum_line
{
  my $enum_dictionary = shift;
  my $entry_re = shift;
  my $line = shift;
  my $orig_line = $line;

  # Skip empty lines
  return "" if($line =~ /^[\s\n\r]*$/);

  dbg_print("\nprocess_enum_line($line, $entry_re)\n");

  $line =~ s/$entry_re/process_enum_entry($enum_dictionary, $1, $2, $3)/xemg;

  if($line eq $orig_line)
  {
    # Drop carriage returns for more readable output
    $orig_line =~ s/[\n\r]//mg;
    error_exit("Unprocessed line found: \"$orig_line\"");
  }
}

sub process_enum
{
  my $enums_array_ref=shift;
  my $enum_entry_re = shift;
  my $text = shift;
  my $binary_type=shift;

  dbg_print("\nprocess_enum($text, $binary_type)\n\n");

  my %enum_dictionary;

  $enum_dictionary{"binary_type"} = $binary_type;

  $text =~ s/^(.*)$/process_enum_line(\%enum_dictionary, $enum_entry_re, $1)/xemg;

  push(@{$enums_array_ref}, \%enum_dictionary);

  return "";
}

sub print_hash_to_ini
{
  my $hash_name = shift;
  my $field_name_prefix = shift;
  my $fhMTIDLC = shift;
  my $dictionary_ref = shift;

  print $fhMTIDLC "[$hash_name]\n" if($hash_name);

  for my $key ( sort keys %{$dictionary_ref} )
  {
    if($key eq "fields")
    {
      my $i;
      foreach (@{${$dictionary_ref}{$key}})
      {
        print_hash_to_ini("", "field_".$i++."_", $fhMTIDLC, $_);
      } 
    }
    else
    {
      my $value = ${$dictionary_ref}{$key};
      print $fhMTIDLC $field_name_prefix.$key."=$value\n";
    }
  }

  print $fhMTIDLC "\n" if($hash_name);
}

print("MTIDL to INI compiler started...\n");

my $usage_text = <<USAGE_END;
The command line is incorrect.
Usage:
    perl mtidl_ini.pl <Output file name> <Input file name 1> <Input file name 2> ... <Input file name N>
USAGE_END

($#ARGV + 1) > 1 or error_exit($usage_text);

my $MTIDLCFile = shift(@ARGV);

my $mtidl_item_re = 'MTIDL_ITEM_START((?: .|\n)*?)MTIDL_ITEM_END\s*\(\s*('.$word_re.')\s*\)';
my $mtidl_enum_re = 'MTIDL_ENUM_START((?: .|\n)*?)MTIDL_ENUM_END\s*\(\s*('.$word_re.')\s*\)';
my $mtidl_bitfield_re = 'MTIDL_BITFIELD_START((?: .|\n)*?)MTIDL_BITFIELD_END\s*\(\s*('.$word_re.')\s*\)';
my @items_array;
my @enums_array;
my @bitfields_array;

my $text;

foreach (@ARGV)
{
  my $fname = $_;
  print("\tReading file $fname... ");

  open my $fh, '<', $fname or die "ERROR: failed to open file $fname for reading: $!";
  my $fcontents = do { local $/; <$fh> };
  $text = $text.$fcontents;

  print("DONE.\n");
}

print("\tCollecting definitions... ");
$text =~ s/$mtidl_item_id_re/process_definition($1, \%mtidl_ids)/xemg;
$text =~ s/$mtidl_item_type_re/process_definition($1, \%mtidl_types)/xemg;
$text =~ s/$mtidl_item_level_re/process_definition($1, \%mtidl_levels)/xemg;
$text =~ s/$mtidl_item_source_re/process_definition($1, \%mtidl_sources)/xemg;
$text =~ s/$mtidl_item_constant_re/process_definition($1, \%mtidl_constants)/xemg;
print("DONE.\n");

print("\tParsing enums... ");
$text =~ s/$mtidl_enum_re/process_enum(\@enums_array, $mtidl_enum_entry_re, $1, $2)/xemg;
print("DONE.\n");

print("\tParsing bitfields... ");
$text =~ s/$mtidl_bitfield_re/process_enum(\@bitfields_array, $mtidl_bitfield_entry_re, $1, $2)/xemg;
print("DONE.\n");

print("\tParsing items... ");
$text =~ s/$mtidl_item_re/process_item(\@items_array, $1, $2)/xemg;
print("DONE.\n");

print("\tSaving output to $MTIDLCFile... ");

open my $fhMTIDLC, '>', $MTIDLCFile.".mtidlc" or error_exit("failed to open $MTIDLCFile for writing: $!");

print_hash_to_ini("ID", "", $fhMTIDLC, \%mtidl_ids) if($debug);
print_hash_to_ini("Type", "", $fhMTIDLC, \%mtidl_types) if($debug);
print_hash_to_ini("Level", "", $fhMTIDLC, \%mtidl_levels) if($debug);
print_hash_to_ini("Sources", "", $fhMTIDLC, \%mtidl_sources) if($debug);
print_hash_to_ini("Constants", "", $fhMTIDLC, \%mtidl_constants) if($debug);

my $i=0;
foreach (@enums_array)
{
  print_hash_to_ini("mtidl_enum_".$i++, "", $fhMTIDLC, $_);
}

$i=0;
foreach (@bitfields_array)
{
  print_hash_to_ini("mtidl_bitfield_".$i++, "", $fhMTIDLC, $_);
}

$i=0;
foreach (@items_array)
{
  print_hash_to_ini("mtidl_item_".$i++, "", $fhMTIDLC, $_);
}

print("DONE.\n");

exit 0;
