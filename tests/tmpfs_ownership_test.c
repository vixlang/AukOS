#include <aukos/log.h>
#include <aukos/memory.h>
#include <aukos/tmpfs.h>
#include <aukos/vfs.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t live_pages;
static size_t allocation_limit = SIZE_MAX;

void log_write(enum log_level level, const char *message)
{
    (void)level;
    (void)message;
}

void *kmalloc(size_t size)
{
    return calloc(1u, size);
}

void *pmm_alloc_page(void)
{
    void *page;

    if (allocation_limit == 0u) {
        return 0;
    }
    if (allocation_limit != SIZE_MAX) {
        allocation_limit--;
    }
    page = aligned_alloc(PAGE_SIZE, PAGE_SIZE);
    if (page) {
        live_pages++;
    }
    return page;
}

void pmm_free_page(void *page)
{
    assert(page);
    assert(live_pages > 0u);
    live_pages--;
    free(page);
}

uint64_t pmm_available_page_count(void)
{
    return UINT64_MAX - live_pages;
}

static void expect_bytes(struct vfs_file *file, size_t offset,
                         const void *expected, size_t size)
{
    uint8_t buffer[32];

    assert(size <= sizeof(buffer));
    assert(vfs_read_at(file, buffer, size, offset) == size);
    assert(memcmp(buffer, expected, size) == 0);
}

int main(void)
{
    struct vfs_file *file;
    struct vfs_file *old_file;
    struct vfs_file *new_file;
    struct vfs_file *open_files[VFS_MAX_OPEN_FILES];
    char node_names[64][32];
    uint8_t zeros[8] = {0};
    uint8_t page_data[PAGE_SIZE * 2u];
    size_t baseline_pages;
    uint64_t seek_result;

    memset(page_data, 0x5a, sizeof(page_data));
    vfs_init();
    assert(vfs_register_filesystem("tmpfs", tmpfs_mount_empty) == 0);
    assert(vfs_mount("/", "tmpfs") == 0);
    assert(vfs_mkdir("/work", 0755u) == 0);

    assert(vfs_open("/work/flags", VFS_O_CREAT | VFS_O_RDWR, 0600u,
                    &file) == 0);
    assert(vfs_write(file, "keep", 4u) == 4u);
    assert(vfs_close(file) == 0);
    assert(vfs_open("/work/flags", VFS_O_CREAT | VFS_O_EXCL |
                    VFS_O_RDWR | VFS_O_TRUNC, 0600u, &file) == -1);
    assert(vfs_open("/work/flags", VFS_O_RDONLY, 0u, &file) == 0);
    assert(vfs_write(file, "x", 1u) == SIZE_MAX);
    expect_bytes(file, 0u, "keep", 4u);
    assert(vfs_close(file) == 0);
    assert(vfs_open("/work/flags", VFS_O_WRONLY, 0u, &file) == 0);
    assert(vfs_read(file, page_data, 1u) == SIZE_MAX);
    assert(vfs_close(file) == 0);

    assert(vfs_open("/work/hole", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &file) == 0);
    assert(vfs_seek(file, 8192, 0u, &seek_result) == 0 && seek_result == 8192u);
    assert(vfs_write(file, "end", 3u) == 3u);
    expect_bytes(file, 8184u, zeros, sizeof(zeros));
    assert(vfs_ftruncate(file, 2u) == 0);
    assert(file->offset == 8195u);
    assert(vfs_ftruncate(file, 8195u) == 0);
    expect_bytes(file, 2u, zeros, sizeof(zeros));
    assert(vfs_close(file) == 0);

    assert(vfs_open("/work/lifetime", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &old_file) == 0);
    assert(vfs_write(old_file, "old", 3u) == 3u);
    assert(vfs_unlink("/work/lifetime") == 0);
    assert(vfs_open("/work/lifetime", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &new_file) == 0);
    assert(vfs_write(new_file, "new", 3u) == 3u);
    expect_bytes(old_file, 0u, "old", 3u);
    expect_bytes(new_file, 0u, "new", 3u);
    assert(vfs_close(old_file) == 0);
    assert(vfs_close(new_file) == 0);

    assert(vfs_open("/work/source", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &file) == 0);
    assert(vfs_write(file, "source", 6u) == 6u);
    assert(vfs_close(file) == 0);
    assert(vfs_open("/work/target", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &old_file) == 0);
    assert(vfs_write(old_file, "target", 6u) == 6u);
    assert(vfs_rename("/work/source", "/work/target") == 0);
    expect_bytes(old_file, 0u, "target", 6u);
    assert(vfs_open("/work/target", VFS_O_RDONLY, 0u, &new_file) == 0);
    expect_bytes(new_file, 0u, "source", 6u);
    assert(vfs_rename("/work/target", "/work/target") == 0);
    assert(vfs_rename("/work/missing", "/work/target") == -1);
    assert(vfs_close(old_file) == 0);
    assert(vfs_close(new_file) == 0);

    for (size_t iteration = 0; iteration < 128u; iteration++) {
        assert(vfs_open("/work/reuse", VFS_O_CREAT | VFS_O_EXCL |
                        VFS_O_RDWR, 0644u, &file) == 0);
        assert(vfs_write(file, &iteration, sizeof(iteration)) ==
               sizeof(iteration));
        assert(vfs_rename("/work/reuse", "/work/reused") == 0);
        assert(vfs_unlink("/work/reused") == 0);
        assert(vfs_close(file) == 0);
    }

    baseline_pages = live_pages;
    assert(vfs_open("/work/large", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &file) == 0);
    for (size_t offset = 0; offset < 256u * 1024u; offset += sizeof(page_data)) {
        assert(vfs_write(file, page_data, sizeof(page_data)) ==
               sizeof(page_data));
    }
    assert(live_pages == baseline_pages + 64u);
    assert(vfs_ftruncate(file, 1u) == 0);
    assert(live_pages == baseline_pages + 1u);
    assert(vfs_ftruncate(file, 256u * 1024u) == 0);
    expect_bytes(file, PAGE_SIZE + 3u, zeros, sizeof(zeros));
    assert(vfs_unlink("/work/large") == 0);
    assert(vfs_close(file) == 0);
    assert(live_pages == baseline_pages);

    assert(vfs_open("/work/rollback", VFS_O_CREAT | VFS_O_RDWR, 0644u,
                    &file) == 0);
    allocation_limit = 1u;
    assert(vfs_write(file, page_data, sizeof(page_data)) == SIZE_MAX);
    assert(file->inode->size == 0u);
    assert(live_pages == baseline_pages);
    allocation_limit = SIZE_MAX;
    assert(vfs_close(file) == 0);

    for (size_t index = 0; index < VFS_MAX_OPEN_FILES; index++) {
        assert(vfs_open("/work/flags", VFS_O_RDONLY, 0u,
                        &open_files[index]) == 0);
    }
    assert(vfs_open_file_count() == VFS_MAX_OPEN_FILES);
    assert(vfs_open("/work/flags", VFS_O_RDONLY, 0u, &file) == -1);
    for (size_t index = 0; index < VFS_MAX_OPEN_FILES; index++) {
        assert(vfs_close(open_files[index]) == 0);
    }
    assert(vfs_open_file_count() == 0u);
    assert(vfs_close(open_files[0]) == -1);

    {
        size_t node_count = 0u;

        while (node_count < 64u) {
            snprintf(node_names[node_count], sizeof(node_names[node_count]),
                     "/work/node%02zu", node_count);
            if (vfs_open(node_names[node_count], VFS_O_CREAT | VFS_O_EXCL |
                         VFS_O_RDWR, 0644u, &file) != 0) {
                break;
            }
            assert(vfs_close(file) == 0);
            node_count++;
        }
        assert(node_count > 0u && node_count < 64u);
        assert(vfs_unlink(node_names[0]) == 0);
        assert(vfs_open("/work/node-reused", VFS_O_CREAT | VFS_O_EXCL |
                        VFS_O_RDWR, 0644u, &file) == 0);
        assert(vfs_close(file) == 0);
        assert(vfs_unlink("/work/node-reused") == 0);
        for (size_t index = 1u; index < node_count; index++) {
            assert(vfs_unlink(node_names[index]) == 0);
        }
    }

    puts("tmpfs_ownership_test: PASS");
    return 0;
}
