#include <stdio.h>
#include <stdlib.h>

#include "hidapi_parser.h"

#define DEBUG_PARSER

//// ---------- HID descriptor parser

// main items
#define HID_INPUT 0x80
#define HID_OUTPUT 0x90
#define HID_COLLECTION 0xA0
#define HID_FEATURE 0xB0
#define HID_END_COLLECTION 0xC0

// HID Report Items from HID 1.11 Section 6.2.2
#define HID_USAGE_PAGE 0x04
#define HID_USAGE 0x08
#define HID_USAGE_MIN 0x18
#define HID_USAGE_MAX 0x28

#define HID_DESIGNATOR_INDEX 0x38
#define HID_DESIGNATOR_MIN 0x48
#define HID_DESIGNATOR_MAX 0x58

#define HID_STRING_INDEX 0x78
#define HID_STRING_MIN 0x88
#define HID_STRING_MAX 0x98

#define HID_DELIMITER 0xA8

#define HID_LOGICAL_MIN 0x14
#define HID_LOGICAL_MAX 0x24

#define HID_PHYSICAL_MIN 0x34
#define HID_PHYSICAL_MAX 0x44

#define HID_UNIT_EXPONENT 0x54
#define HID_UNIT 0x64

#define HID_REPORT_SIZE 0x74
#define HID_REPORT_ID 0x84

#define HID_REPORT_COUNT 0x94

#define HID_PUSH 0xA4
#define HID_POP 0xB4

#define HID_RESERVED 0xC4 // above this it is all reserved


// HID Report Usage Pages from HID Usage Tables 1.12 Section 3, Table 1
// #define HID_USAGE_PAGE_GENERICDESKTOP  0x01
// #define HID_USAGE_PAGE_KEY_CODES       0x07
// #define HID_USAGE_PAGE_LEDS            0x08
// #define HID_USAGE_PAGE_BUTTONS         0x09

// HID Report Usages from HID Usage Tables 1.12 Section 4, Table 6
// #define HID_USAGE_POINTER  0x01
// #define HID_USAGE_MOUSE    0x02
// #define HID_USAGE_JOYSTICK 0x04
// #define HID_USAGE_KEYBOARD 0x06
// #define HID_USAGE_X        0x30
// #define HID_USAGE_Y        0x31
// #define HID_USAGE_Z        0x32
// #define HID_USAGE_RX       0x33
// #define HID_USAGE_RY       0x34
// #define HID_USAGE_RZ       0x35
// #define HID_USAGE_SLIDER   0x36
// #define HID_USAGE_DIAL     0x37
// #define HID_USAGE_WHEEL    0x38


// HID Report Collection Types from HID 1.12 6.2.2.6
#define HID_COLLECTION_PHYSICAL    0x00
#define HID_COLLECTION_APPLICATION 0x01
#define HID_COLLECTION_LOGICAL 0x02
#define HID_COLLECTION_REPORT 0x03
#define HID_COLLECTION_NAMED_ARRAY 0x04
#define HID_COLLECTION_USAGE_SWITCH 0x05
#define HID_COLLECTION_USAGE_MODIFIER 0x06
#define HID_COLLECTION_RESERVED 0x07
#define HID_COLLECTION_VENDOR 0x80

// HID Input/Output/Feature Item Data (attributes) from HID 1.11 6.2.2.5
/// more like flags - for input, output, and feature
#define HID_ITEM_CONSTANT 0x1 // data(0), constant(1)
#define HID_ITEM_VARIABLE 0x2 // array(0), variable(1)
#define HID_ITEM_RELATIVE 0x4 // absolute(0), relative(1)
#define HID_ITEM_WRAP 0x8 // no wrap(0), wrap(1)
#define HID_ITEM_LINEAR 0x10 // linear(0), non linear(1)
#define HID_ITEM_PREFERRED 0x20 // no preferred(0), preferred(1)
#define HID_ITEM_NULL 0x40 // no null(0), null(1)
#define HID_ITEM_VOLATILE 0x60 // non volatile(0), volatile(1)
#define HID_ITEM_BITFIELD 0x80 // bit field(0), buffered bytes(1)

// Report Types from HID 1.11 Section 7.2.1
#define HID_REPORT_TYPE_INPUT   1
#define HID_REPORT_TYPE_OUTPUT  2
#define HID_REPORT_TYPE_FEATURE 3


#define BITMASK1(n) ((1ULL << (n)) - 1ULL)

int hid_parse_report_descriptor( char* descr_buf, int size, struct hid_device_descriptor * descriptor ){
  struct hid_device_element * prev_element;
  int current_usage_page;
  int current_usage;
  int current_usages[256];
  int current_usage_index = 0;
  int current_usage_min = -1;
  int current_usage_max = -1;
  int current_logical_min;
  int current_logical_max;
  int current_physical_min;
  int current_physical_max;
  int current_report_count;
  int current_report_id;
  int current_report_size;
  char current_input;
  char current_output;
  int collection_nesting = 0;
  
  int next_byte_tag = -1;
  int next_byte_size = 0;
  int next_byte_type = 0;
  int next_val = 0;
  
  int toadd = 0;
  int byte_count = 0;
  
  int i,j;
  
  descriptor->num_elements = 0;
  for ( i = 0; i < size; i++){
#ifdef DEBUG_PARSER
	  printf("\n%02hhx ", descr_buf[i]);
	  printf("\tbyte_type %i, %i, %i \t", next_byte_tag, next_byte_size, next_val);
#endif
	  if ( next_byte_tag != -1 ){
	      toadd = descr_buf[i];
	      for ( j=0; j<byte_count; j++ ){
		  toadd = toadd << 8;
	      }
	      next_val += toadd;
	      byte_count++;
	      if ( byte_count == next_byte_size ){
		switch( next_byte_tag ){
		  case HID_USAGE_PAGE:
		    current_usage_page = next_val;
#ifdef DEBUG_PARSER
		    printf("usage page: 0x%02hhx", current_usage_page);
#endif
		    break;
		  case HID_USAGE:
		    current_usage = next_val;
		    current_usages[ current_usage_index ] = next_val;
#ifdef DEBUG_PARSER
		    printf("usage: 0x%02hhx, %i", current_usages[ current_usage_index ], current_usage_index );
#endif
		    current_usage_index++;
		    break;
		  case HID_COLLECTION:
		    //TODO: COULD ALSO READ WHICH KIND OF COLLECTION
		    collection_nesting++;
#ifdef DEBUG_PARSER
		    printf("collection: %i, %i", collection_nesting, next_val );
#endif
		    break;
		  case HID_USAGE_MIN:
		    current_usage_min = next_val;
#ifdef DEBUG_PARSER
		    printf("usage min: %i", current_usage_min);
#endif
		    break;
		  case HID_USAGE_MAX:
		    current_usage_max = next_val;
#ifdef DEBUG_PARSER
		    printf("usage max: %i", current_usage_max);
#endif
		    break;
		  case HID_LOGICAL_MIN:
		    current_logical_min = next_val;
#ifdef DEBUG_PARSER
		    printf("logical min: %i", current_logical_min);
#endif
		    break;
		  case HID_LOGICAL_MAX:
		    current_logical_max = next_val;
#ifdef DEBUG_PARSER
		    printf("logical max: %i", current_logical_max);
#endif
		    break;
		  case HID_PHYSICAL_MIN:
		    current_physical_min = next_val;
#ifdef DEBUG_PARSER
		    printf("physical min: %i", current_physical_min);
#endif
		    break;
		  case HID_PHYSICAL_MAX:
		    current_physical_max = next_val;
#ifdef DEBUG_PARSER
		    printf("physical max: %i", current_physical_min);
#endif
		    break;
		  case HID_REPORT_COUNT:
		    current_report_count = next_val;
#ifdef DEBUG_PARSER
		    printf("report count: %i", current_report_count);
#endif
		    break;
		  case HID_REPORT_SIZE:
		    current_report_size = next_val;
#ifdef DEBUG_PARSER
		    printf("report size: %i", current_report_size);
#endif
		    break;
		  case HID_REPORT_ID:
		    current_report_id = next_val;
#ifdef DEBUG_PARSER
		    printf("report id: %i", current_report_id);
#endif
		    break;
		  case HID_POP:
		    // TODO: something useful with pop
#ifdef DEBUG_PARSER
		    printf("pop: %i", next_val );
#endif
		    break;
		  case HID_PUSH:
		    // TODO: something useful with push
#ifdef DEBUG_PARSER
		    printf("pop: %i", next_val );
#endif
		    break;
		  case HID_UNIT:
		    // TODO: something useful with unit information
#ifdef DEBUG_PARSER
		    printf("unit: %i", next_val );
#endif
		    break;
		  case HID_UNIT_EXPONENT:
		    // TODO: something useful with unit exponent information
#ifdef DEBUG_PARSER
		    printf("unit exponent: %i", next_val );
#endif
		    break;
		  case HID_INPUT:
#ifdef DEBUG_PARSER
		    printf("input: %i", next_val);
#endif
		    // add the elements for this report
		    for ( j=0; j<current_report_count; j++ ){
			struct hid_device_element * new_element = (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
			new_element->index = descriptor->num_elements;
			new_element->io_type = 1;
			new_element->type = next_val; //TODO: parse this for more detailed info

			new_element->usage_page = current_usage_page;
			if ( current_usage_min != -1 ){
			  new_element->usage = current_usage_min + j;
			} else {
			  new_element->usage = current_usages[j];
			}
			new_element->logical_min = current_logical_min;
			new_element->logical_max = current_logical_max;
			new_element->phys_min = current_physical_min;
			new_element->phys_max = current_physical_max;
			
			new_element->report_size = current_report_size;
			new_element->report_id = current_report_id;
			new_element->report_index = j;
			
			new_element->value = 0;
			if ( descriptor->num_elements == 0 ){
			    descriptor->first = new_element;
			} else {
			    prev_element->next = new_element;
			}
			descriptor->num_elements++;
			prev_element = new_element;
		    }
		    current_usage_min = -1;
		    current_usage_max = -1;
		    current_usage_index = 0;
		    break;
		  case HID_OUTPUT:
#ifdef DEBUG_PARSER
		    printf("output: %i", next_val);
#endif
		    		    // add the elements for this report
		    for ( j=0; j<current_report_count; j++ ){
			struct hid_device_element * new_element = (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
			new_element->index = descriptor->num_elements;
			new_element->io_type = 2;
			new_element->type = next_val; //TODO: parse this for more detailed info

			new_element->usage_page = current_usage_page;
			if ( current_usage_min != -1 ){
			  new_element->usage = current_usage_min + j;
			} else {
			  new_element->usage = current_usages[j];
			}
			new_element->logical_min = current_logical_min;
			new_element->logical_max = current_logical_max;
			new_element->phys_min = current_physical_min;
			new_element->phys_max = current_physical_max;
			
			new_element->report_size = current_report_size;
			new_element->report_id = current_report_id;
			new_element->report_index = j;
			
			new_element->value = 0;
			if ( descriptor->num_elements == 0 ){
			    descriptor->first = new_element;
			} else {
			    prev_element->next = new_element;
			}
			descriptor->num_elements++;
			prev_element = new_element;
		    }
		    current_usage_min = -1;
		    current_usage_max = -1;
		    current_usage_index = 0;
		    break;
		  case HID_FEATURE:
#ifdef DEBUG_PARSER
		    printf("feature: %i", next_val);
#endif
		    // add the elements for this report
		    for ( j=0; j<current_report_count; j++ ){
			struct hid_device_element * new_element = (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
			new_element->index = descriptor->num_elements;
			new_element->io_type = 3;
			new_element->type = next_val; //TODO: parse this for more detailed info

			new_element->usage_page = current_usage_page;
			if ( current_usage_min != -1 ){
			  new_element->usage = current_usage_min + j;
			} else {
			  new_element->usage = current_usages[j];
			}
			new_element->logical_min = current_logical_min;
			new_element->logical_max = current_logical_max;
			new_element->phys_min = current_physical_min;
			new_element->phys_max = current_physical_max;
			
			new_element->report_size = current_report_size;
			new_element->report_id = current_report_id;
			new_element->report_index = j;
			
			new_element->value = 0;
			if ( descriptor->num_elements == 0 ){
			    descriptor->first = new_element;
			} else {
			    prev_element->next = new_element;
			}
			descriptor->num_elements++;
			prev_element = new_element;
		    }
		    current_usage_min = -1;
		    current_usage_max = -1;
		    current_usage_index = 0;
		    break;
#ifdef DEBUG_PARSER
		  default:
		    if ( next_byte_tag >= HID_RESERVED ){
		      printf("reserved bytes 0x%02hhx, %i", next_byte_tag, next_val );
		    } else {
		      printf("undefined byte type 0x%02hhx, %i", next_byte_tag, next_val );
		    }
#endif
		}
	      next_byte_tag = -1;
	      }
	  } else {
#ifdef DEBUG_PARSER
	    printf("\tsetting next byte type: %i, 0x%02hhx ", descr_buf[i], descr_buf[i] );
#endif
	    if ( descr_buf[i] == HID_END_COLLECTION ){ // JUST one byte
	      collection_nesting--;
#ifdef DEBUG_PARSER
	      printf("\tend collection: %i, %i\n", collection_nesting, descr_buf[i] );
#endif
	    } else {
	      byte_count = 0;
	      next_val = 0;
	      next_byte_tag = descr_buf[i] & 0xFC;
	      next_byte_type = descr_buf[i] & 0x0C;
	      next_byte_size = descr_buf[i] & 0x03;
	      if ( next_byte_size == 3 ){
		  next_byte_size = 4;
	      }
#ifdef DEBUG_PARSER
	      printf("\t next byte type:  0x%02hhx, %i, %i ", next_byte_tag, next_byte_type, next_byte_size );
#endif
	    }
	  }
  }
  return 0;
}

float hid_element_map_logical( struct hid_device_element * element ){
    return 0.;
}

float hid_element_map_physical( struct hid_device_element * element ){
    return 0.;
}

int hid_parse_input_report( char* buf, int size, struct hid_device_descriptor * descriptor ){
  
  return 0;
}
