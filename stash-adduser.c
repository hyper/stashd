//-----------------------------------------------------------------------------
// stash-adduser
//	Tool to add a user to the stash.  It will be able to add a user directly to 
//	the stash files if it is not currently loaded, or it can connect and add a 
//	user through the admin interface (requires an existing user with admin 
//	privs).
//-----------------------------------------------------------------------------


// includes
#include "stash-common.h"
#include <stash.h>

#include <assert.h>
#include <expbuf.h>
#include <risp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>



#define PACKAGE						"stash-adduser"
#define VERSION						"0.10"



//-----------------------------------------------------------------------------
// global variables.

short int verbose = 0;





//-----------------------------------------------------------------------------
// print some info to the user, so that they can know what the parameters do.
static void usage(void) {
	printf(PACKAGE " " VERSION "\n");
	printf("Required params:\n");
	printf(" -u <username>      new username\n");
	printf(" -p <password>      new password\n");
	printf("\n");
	printf("Direct file method:\n");
	printf(" -d <path>          storage path\n");
	printf("\n");
	printf("Connect to running instance method:\n");
	printf(" -H <host:port>     Hostname of the running instance.\n");
	printf(" -U <username>      Admin username\n");
	printf(" -P <password>      Admin password\n");
	printf("\n");
	printf("Misc. Options:\n");
	printf(" -v                 verbose\n");
	printf(" -h                 print this help and exit\n");
	return;
}





//-----------------------------------------------------------------------------
// Main... process command line parameters, and if we have enough information, then create an empty stash.
int main(int argc, char **argv) 
{
	int c;
	storage_t *storage = NULL;
	stash_t *stash = NULL;
	int result;
	const char *basedir = NULL;
	const char *newuser = NULL;
	const char *newpass = NULL;
	const char *host = NULL;
	const char *username = NULL;
	const char *password = NULL;
	userid_t uid;
	user_t *user;
	stash_result_t res;
	
	
	assert(argc >= 0);
	assert(argv);
	
	// process arguments
	/// Need to check the options in here, there're possibly ones that we dont need.
	while ((c = getopt(argc, argv, "hvd:u:p:H:U:P:")) != -1) {
		switch (c) {
			case 'h':
				usage();
				exit(EXIT_SUCCESS);
			case 'v':
				verbose++;
				break;
			case 'd':
				basedir = optarg;
				break;
			case 'u':
				newuser = optarg;
				break;
			case 'p':
				newpass = optarg;
				break;

			case 'H':
				host = optarg;
				break;
			case 'U':
				username = optarg;
				break;
			case 'P':
				password = optarg;
				break;
				
			default:
				fprintf(stderr, "Illegal argument \"%c\"\n", c);
				exit(1);
				
		}
	}
	
	// check that our required params are there:
	if (basedir == NULL && host == NULL) {
		fprintf(stderr, "missing required option, either -d or -H\n");
		exit(1);
	}
	else if (basedir && host) {
		fprintf(stderr, "cannot specify both a directory and a host.\n");
		exit(1);
	}
	else if (newuser == NULL) {
		fprintf(stderr, "missing required parameter: -u\n");
		exit(1);
	}
	
	result = 0;
	
	if (basedir) {
		// we are using a basedir direct method.  For this we will need to use 
		// the stash_storage functionality (that is part of the libstash 
		// library).
		
		storage = storage_init(NULL);
		assert(storage);
		
		// process the main meta file;
		assert(basedir);
		storage_lock_master(storage, basedir);
		storage_process(storage, basedir, KEEP_OPEN, IGNORE_DATA);
		
		// if the namespace is available, then create it.
		if (storage_username_avail(storage, newuser) == 0) {
			fprintf(stderr, "Username '%s' is already in use.\n", newuser);
			result = 1;
		}
		else {
			user = storage_create_username(storage, NULL_USER_ID, newuser);
			assert(user);
			assert(user->uid > 0);
			if (newpass) {
				storage_set_password(storage, NULL_USER_ID, user->uid, newpass);
			}

			if (verbose) {
				printf("Username '%s' created.\n", newuser);
				assert(result == 0);
			}
		}
		
		storage_unlock_master(storage, basedir);
		
		// cleanup the storage 
		storage_free(storage);
		storage = NULL;		
	}
	else {
		// network version.
		
		stash = stash_init(NULL);
		assert(stash);
		
		// add our username and password to the authority... in future 
		// versions, private and public keys may be used instead.
		assert(username);
		assert(password);
		stash_authority(stash, username, password);

		// add our known host to the server list.
		assert(host);
		stash_addserver(stash, host, 10);
		
		// connect to the database... check error code.
		// although it is not necessary to connect now, because if we dont, 
		// the first operation we do will attempt to connect if we are not 
		// already connected.  However, it is useful right now to attempt to 
		// connect so that we can report the error back to the user.  Easier 
		// to do it here and it doesn't cost anything.
		res = stash_connect(stash);
		if (res != 0) {
			fprintf(stderr, "Unable to connect: %04X:%s\n", res, stash_err_text(res));
		}
		else {

			// attempt to create the user id.
			uid = 0;
			res = stash_create_username(stash, newuser, &uid);
			if (res != STASH_ERR_OK) {
				switch(res) {
					case STASH_ERR_USEREXISTS:
						fprintf(stderr, "Username '%s' is already in use.\n", newuser);
						break;
					default:
						fprintf(stderr, "Unexpected error: %04X:%s\n", res, stash_err_text(res));
						break;
				}
				result = 1;
			}
			else {
				
				assert(uid > 0);
				
				if (newpass) {
					res = stash_set_password(stash, uid, NULL, newpass);
					assert(res == 0);
				}
				
				if (verbose) {
					printf("Username '%s' created.\n", newuser);
				}
				assert(result == 0);
			}
		}
		
		// cleanup the storage 
		stash_free(stash);
		stash = NULL;		
	}

	assert(stash == NULL);
	assert(storage == NULL);
	
	return(result);
}


