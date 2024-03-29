#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  buffer_cache_init ();
  if (format) 
    do_format ();

  free_map_open ();
  thread_current () ->cwd = dir_open_root ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  buffer_cache_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  char copy_name [strlen (name) + 1];
  char file_name [strlen (name) + 1];
  memcpy (copy_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (copy_name, file_name);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

bool
filesys_create_dir (const char *name)
{
  block_sector_t inode_sector = 0;
  char copy_name [strlen (name) + 1];
  char file_name [strlen (name) + 1];
  memcpy (copy_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (copy_name, file_name);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);

  if (success)
    {
      struct dir *new_dir = dir_open (inode_open (inode_sector));
      dir_add (new_dir, ".", inode_sector);
      dir_add (new_dir, "..", inode_get_inumber (dir_get_inode (dir)));
      dir_close (new_dir);
    }
  dir_close (dir);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char copy_name [strlen (name) + 1];
  char file_name [strlen (name) + 1];
  memcpy (copy_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (copy_name, file_name);
  struct inode *inode = NULL;

  if (dir == NULL) return NULL;
  if (strlen (file_name) > 0) {
    dir_lookup (dir, file_name, &inode);
    dir_close (dir);
  } else {
    inode = dir_get_inode (dir);
  }
  if (inode == NULL) return NULL;
  if (inode_is_removed (inode)) return NULL;
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char copy_name [strlen (name) + 1];
  char file_name [strlen (name) + 1];
  memcpy (copy_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (copy_name, file_name);

  struct inode *inode;
  dir_lookup (dir, file_name, &inode);
  struct dir *cur_dir = NULL;
  char temp[257];

  bool success = false;
  if (!inode_is_dir (inode) ||
    ((cur_dir = dir_open (inode)) && !dir_readdir (cur_dir, temp)))
    success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir);

  if (cur_dir)
    dir_close (cur_dir);
  return success;
}

bool
filesys_chdir (const char *name)
{
  char copy_name [strlen (name) + 1];
  memcpy (copy_name, name, strlen (name) + 1);
  struct dir *dir = parse_path_for_dir (copy_name);
  if (dir == NULL) return false;
  dir_close (thread_current () ->cwd);
  thread_current () ->cwd = dir;
  return true;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  struct dir *root = dir_open_root ();
  dir_add (root, ".", ROOT_DIR_SECTOR);
  dir_add (root, "..", ROOT_DIR_SECTOR);  
  dir_close (root);
  free_map_close ();
  printf ("done.\n");
}

struct dir*
parse_path (char *path, char* file_name)
{
  struct dir* dir;
  if (path == NULL || file_name == NULL) {
    return NULL;
  }
  if (strlen (path) == 0) {
    return NULL;
  }
  if (path[0] == '/') {
    dir = dir_open_root ();
  } else {
    if (thread_current () ->cwd == NULL) {
      dir = dir_open_root ();
    } else {
      dir = dir_reopen (thread_current () ->cwd);
    }
  }

  if (inode_is_removed (dir_get_inode(dir))) {
    dir_close(dir);
    return NULL;
  }

  char *token, *next, *ptr;
  token = strtok_r (path, "/", &ptr);
  next = strtok_r (NULL, "/", &ptr);

  if (!token) {
    memcpy (file_name, ".", 2);
    return dir;
  }
  while (token != NULL && next != NULL) {
    struct inode *inode = NULL;
    if (!dir_lookup (dir, token, &inode)) {
      dir_close (dir);
      return NULL;
    }
    if (!inode_is_dir (inode)) {
      dir_close (dir);
      return NULL;
    }
    dir_close (dir);
    dir = dir_open (inode);

    token = next;
    next = strtok_r (NULL, "/", &ptr);
  }
  memcpy (file_name, token, strlen(token) + 1);
  return dir;
}

struct dir*
parse_path_for_dir (char *path)
{
  struct dir* dir;
  if (path == NULL) {
    return NULL;
  }
  if (strlen (path) == 0) {
    return NULL;
  }
  if (path[0] == '/') {
    dir = dir_open_root ();
  } else {
    if (thread_current () ->cwd == NULL) {
      dir = dir_open_root ();
    } else {
      dir = dir_reopen (thread_current () ->cwd);
    }
  }
  char *token, *next, *ptr;
  token = strtok_r (path, "/", &ptr);

  while (token != NULL) {
    struct inode *inode = NULL;
    if (!dir_lookup (dir, token, &inode)) {
      dir_close (dir);
      return NULL;
    }
    if (!inode_is_dir (inode)) {
      dir_close (dir);
      return NULL;
    }
    dir_close (dir);
    dir = dir_open (inode);
    if (dir == NULL) {
      dir_close (dir);
      return NULL;
    }
    token = strtok_r (NULL, "/", &ptr);
  }
  if (inode_is_removed (dir_get_inode(dir))) {
    dir_close(dir);
    return NULL;
  }
  return dir;
}
