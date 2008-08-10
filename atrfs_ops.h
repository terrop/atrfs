#ifndef ATRFS_OPS_H
#define ATRFS_OPS_H

extern void atrfs_getlk(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi, struct flock *lock);
extern void atrfs_setlk(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi, struct flock *lock, int sleep);
extern void atrfs_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx);
extern void atrfs_init(void *userdata, struct fuse_conn_info *conn);
extern void atrfs_destroy(void *userdata);
extern void atrfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
extern void atrfs_forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);
extern void atrfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_setattr(fuse_req_t req, fuse_ino_t ino,
	struct stat *attr, int to_set, struct fuse_file_info *fi);
extern void atrfs_readlink(fuse_req_t req, fuse_ino_t ino);
extern void atrfs_mknod(fuse_req_t req, fuse_ino_t parent,
	const char *name, mode_t mode, dev_t rdev);
extern void atrfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
extern void atrfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
extern void atrfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
extern void atrfs_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);
extern void atrfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);
extern void atrfs_rename(fuse_req_t req, fuse_ino_t parent,
	const char *name, fuse_ino_t newparent, const char *newname);
extern void atrfs_create(fuse_req_t req, fuse_ino_t parent, const char *name,
	mode_t mode, struct fuse_file_info *fi);
extern void atrfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_read(fuse_req_t req, fuse_ino_t ino, size_t size,
	off_t off, struct fuse_file_info *fi);
extern void atrfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
	size_t size, off_t off, struct fuse_file_info *fi);
extern void atrfs_statfs(fuse_req_t req, fuse_ino_t ino);
extern void atrfs_access(fuse_req_t req, fuse_ino_t ino, int mask);
extern void atrfs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info *fi);
extern void atrfs_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync,
	struct fuse_file_info *fi);
extern void atrfs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
extern void atrfs_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
	const char *value, size_t size, int flags);
extern void atrfs_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
extern void atrfs_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size);
extern void atrfs_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name);

extern struct fuse_lowlevel_ops atrfs_operations;

#endif /* ATRFS_OPS_H */
