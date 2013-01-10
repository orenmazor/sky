#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "types.h"
#include "delete_table_message.h"
#include "table.h"
#include "minipack.h"
#include "mem.h"
#include "dbg.h"


//==============================================================================
//
// Definitions
//
//==============================================================================

#define SKY_DELETE_TABLE_MESSAGE_TABLE_KEY_COUNT 1

struct tagbstring SKY_DELETE_TABLE_MESSAGE_NAME_STR = bsStatic("name");


//==============================================================================
//
// Functions
//
//==============================================================================

//--------------------------------------
// Lifecycle
//--------------------------------------

// Creates an 'delete_table' message object.
//
// Returns a new message.
sky_delete_table_message *sky_delete_table_message_create()
{
    sky_delete_table_message *message = NULL;
    message = calloc(1, sizeof(sky_delete_table_message)); check_mem(message);
    return message;

error:
    sky_delete_table_message_free(message);
    return NULL;
}

// Frees a 'delete_table' message object from memory.
//
// message - The message object to be freed.
//
// Returns nothing.
void sky_delete_table_message_free(sky_delete_table_message *message)
{
    if(message) {
        free(message);
    }
}



//--------------------------------------
// Message Handler
//--------------------------------------

// Creates a message handler for the message.
//
// Returns a message handler.
sky_message_handler *sky_delete_table_message_handler_create()
{
    sky_message_handler *handler = sky_message_handler_create(); check_mem(handler);
    handler->scope = SKY_MESSAGE_HANDLER_SCOPE_SERVER;
    handler->name = bfromcstr("delete_table");
    handler->process = sky_delete_table_message_process;
    return handler;

error:
    sky_message_handler_free(handler);
    return NULL;
}

// Deletes a table to the server. This function is synchronous and does not use
// a worker.
//
// server - The server.
// header - The message header.
// table  - The table the message is working against
// input  - The input file stream.
// output - The output file stream.
//
// Returns 0 if successful, otherwise returns -1.
int sky_delete_table_message_process(sky_server *server,
                                     sky_message_header *header,
                                     sky_table *_table, FILE *input, FILE *output)
{
    int rc = 0;
    size_t sz;
    sky_delete_table_message *message = NULL;
    sky_table *table = NULL;
    bstring path = NULL;
    check(server != NULL, "Server required");
    check(header != NULL, "Message header required");
    check(input != NULL, "Input stream required");
    check(output != NULL, "Output stream required");
    (void)_table;
    
    struct tagbstring status_str = bsStatic("status");
    struct tagbstring ok_str = bsStatic("ok");

    // Parse message.
    message = sky_delete_table_message_create(); check_mem(message);
    rc = sky_delete_table_message_unpack(message, input);
    check(rc == 0, "Unable to parse 'delete_table' message");

    // Retrieve table reference from server.
    rc = sky_server_get_table(server, message->name, &table);
    check(rc == 0, "Unable to find table: %s", bdata(message->name));
    check(table != NULL, "Table does not exist: %s", bdata(message->name));

    // Detach table first.
    path = bstrcpy(table->path); check_mem(path);
    rc = sky_server_close_table(server, table);
    check(rc == 0, "Unable to close table before deletion");

    // If the table exists then delete it.
    if(sky_file_exists(path)) {
        rc = sky_file_rm_r(path);
        check(rc == 0, "Unable to delete table: %s", bdata(path));
    }
    
    // Return.
    //   {status:"OK"}
    minipack_fwrite_map(output, 2, &sz);
    check(sz > 0, "Unable to write output");
    check(sky_minipack_fwrite_bstring(output, &status_str) == 0, "Unable to write status key");
    check(sky_minipack_fwrite_bstring(output, &ok_str) == 0, "Unable to write status value");

    fclose(input);
    fclose(output);
    bdestroy(path);
    sky_delete_table_message_free(message);
    
    return 0;

error:
    if(input) fclose(input);
    if(output) fclose(output);
    bdestroy(path);
    sky_delete_table_message_free(message);
    return -1;
}


//--------------------------------------
// Serialization
//--------------------------------------

// Serializes an 'delete_table' message to a file stream.
//
// message - The message.
// file    - The file stream to write to.
//
// Returns 0 if successful, otherwise returns -1.
int sky_delete_table_message_pack(sky_delete_table_message *message, FILE *file)
{
    size_t sz;
    check(message != NULL, "Message required");
    check(file != NULL, "File stream required");

    // Map
    minipack_fwrite_map(file, SKY_DELETE_TABLE_MESSAGE_TABLE_KEY_COUNT, &sz);
    check(sz > 0, "Unable to write map");
    
    // Name
    check(sky_minipack_fwrite_bstring(file, &SKY_DELETE_TABLE_MESSAGE_NAME_STR) == 0, "Unable to write table name key");
    check(sky_minipack_fwrite_bstring(file, message->name) == 0, "Unable to write name value");

    return 0;

error:
    return -1;
}

// Deserializes an 'delete_table' message from a file stream.
//
// message - The message.
// file    - The file stream to read from.
//
// Returns 0 if successful, otherwise returns -1.
int sky_delete_table_message_unpack(sky_delete_table_message *message, FILE *file)
{
    int rc;
    size_t sz;
    bstring key = NULL;
    check(message != NULL, "Message required");
    check(file != NULL, "File stream required");

    // Map
    uint32_t map_length = minipack_fread_map(file, &sz);
    check(sz > 0, "Unable to read map");
    
    // Map items
    uint32_t i;
    for(i=0; i<map_length; i++) {
        rc = sky_minipack_fread_bstring(file, &key);
        check(rc == 0, "Unable to read map key");
        
        if(biseq(key, &SKY_DELETE_TABLE_MESSAGE_NAME_STR)) {
            rc = sky_minipack_fread_bstring(file, &message->name);
            check(rc == 0, "Unable to read table name");
        }

        bdestroy(key);
    }
    
    return 0;

error:
    bdestroy(key);
    return -1;
}
