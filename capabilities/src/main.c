
#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4platsupport/bootinfo.h>
#include <utils/util.h>

void capability();
void untype();
int main(int argc, char *argv[]) {

    untype();
    return 0;
}

void capability() {
    /* parse the location of the seL4_BootInfo data structure from
    the environment variables set up by the default crt0.S */
    /* 获取并解析启动信息 */
    seL4_BootInfo *info = platsupport_get_bootinfo();

    /* 计算并打印初始 CNode 的大小 */
    size_t initial_cnode_object_size = BIT(info->initThreadCNodeSizeBits); // 将 1 左移 n 位。
    printf("Initial CNode is %zu slots in size\n", initial_cnode_object_size);

    /* test 1 计算 CNode 的字节数 */
    size_t initial_cnode_object_size_bytes = initial_cnode_object_size * (1 << seL4_SlotBits); // TODO calculate this.
    printf("The CNode is %zu bytes in size\n", initial_cnode_object_size_bytes);

    /* 复制 TCB 能力 */
    /*
     *  从源空间复制一个能力到目标空间。
     *  _service: 目标空间的根节点。
     *  dest_index: 目标空间中的目标槽位索引。
     *  dest_depth: 解析目标槽位所需的索引位数。
     *  src_root: 源空间的根节点。
     *  src_index: 源空间中的源槽位索引。
     *  src_depth: 解析源槽位所需的索引位数。
     *  rights: 新能力的访问权限。
    */
    seL4_CPtr first_free_slot = info->empty.start;  //拿到第一个空的插槽
    seL4_Error error = seL4_CNode_Copy(seL4_CapInitThreadCNode, first_free_slot, seL4_WordBits,
                                       seL4_CapInitThreadCNode, seL4_CapInitThreadTCB, seL4_WordBits,
                                       seL4_AllRights);
    ZF_LOGF_IF(error, "Failed to copy cap!");


    /* test 2 */
    seL4_CPtr last_slot = info->empty.end - 1;      //拿到最后一个插槽
    /* TODO use seL4_CNode_Copy to make another copy of the initial TCB capability to the last slot in the CSpace */
    /* 将 root task 的操作 TCB 的 capability 复制到 最后一个空闲的 Slot 上。*/
    error = seL4_CNode_Copy(seL4_CapInitThreadCNode, last_slot, seL4_WordBits,
                            seL4_CapInitThreadCNode, seL4_CapInitThreadTCB, seL4_WordBits, 
                            seL4_AllRights);
    /* set the priority of the root task */
    error = seL4_TCB_SetPriority(last_slot, last_slot, 10);
    ZF_LOGF_IF(error, "Failed to set priority");

    /* test 3. 删除拷贝的cap */
    // TODO delete the created TCB capabilities
    seL4_CNode_Delete(seL4_CapInitThreadCNode, first_free_slot, seL4_WordBits);
    seL4_CNode_Delete(seL4_CapInitThreadCNode, last_slot, seL4_WordBits);

    /* use Revoke to delete the TCB */
    // seL4_CNode_Revoke(seL4_CapInitThreadCNode, seL4_CapInitThreadTCB, seL4_WordBits);
   
    // check first_free_slot is empty 检查第一个空闲槽位
    error = seL4_CNode_Move(seL4_CapInitThreadCNode, first_free_slot, seL4_WordBits,
                            seL4_CapInitThreadCNode, first_free_slot, seL4_WordBits);
    ZF_LOGF_IF(error != seL4_FailedLookup, "first_free_slot is not empty");

    // check last_slot is empty 检查最后一个槽位状态
    error = seL4_CNode_Move(seL4_CapInitThreadCNode, last_slot, seL4_WordBits,
                            seL4_CapInitThreadCNode, last_slot, seL4_WordBits);
    ZF_LOGF_IF(error != seL4_FailedLookup, "last_slot is not empty");

    /* test 4 调用 */
    printf("Suspending current thread\n");
    // TODO suspend the current thread
    seL4_TCB_Suspend(seL4_CapInitThreadTCB);
    ZF_LOGF("Failed to suspend current thread\n");
}
void untype() {
    /* 获取并解析启动信息 */
    seL4_BootInfo *info = platsupport_get_bootinfo();

    printf("    CSlot   \tPaddr           \tSize\tType\n");
    for (seL4_CPtr slot = info->untyped.start; slot != info->untyped.end; slot++) {
        seL4_UntypedDesc *desc = &info->untypedList[slot - info->untyped.start];
        printf("%8p\t%16p\t2^%d\t%s\n", (void *) slot, (void *) desc->paddr, desc->sizeBits, desc->isDevice ? "device untyped" : "untyped");
    }
}