# sel4 capability system

## BackGround
[官方教程](https://docs.sel4.systems/Tutorials/capabilities.html#what-is-a-capability)
**capability space:**

CSpace（capability-space）是一个线程可以访问的全部能力，它可以由一个或多个CNode组成。seL4 实现了基于能力的访问控制模型。每个用户空间线程都有一个关联的能力空间（CSpace），其中包含线程拥有的能力，从而控制线程可以访问哪些资源。seL4 要求程序员从用户空间管理所有内核数据结构，包括 CSpace。这意味着用户空间程序员负责构建 CSpace 以及其中的寻址功能。

**三种 capability:** 

1.内核对象（如 线程控制块）的访问控制的 capability <br/>
2.抽象资源（如 IRQControl ）的访问控制的 capability <br/>
3.untyped capabilities ，负责内存分配的 capabilities, seL4 不支持动态内存分配，在内核加载阶段将所有的空闲内存绑定到 untyped capabilities 上，后续的内核对象申请新的内存，都通过系统调用和 untyped capability 来申请。<br/>

在 seL4 内核初始化时，指向内核控制资源的所有的 capability 全部被授权给特殊的任务 root task ，用户代码想要修改某个资源的状态，必须使用 kernel API 在指定的 capability 上请求操作。root task 拥有控制它自己的进程控制块（TCB）的 capability ，这个 capability 被定义为一个常量 seL4_CapInitThreadTCB ，下面的一个例子就是通过这个 capability 来改变当前当前进程任务的栈顶指针（当需要一个很大的用户栈时，这个操作非常有用）。
```
 seL4_UserContext registers;
    seL4_Word num_registers = sizeof(seL4_UserContext)/sizeof(seL4_Word);

    /* Read the registers of the TCB that the capability in seL4_CapInitThreadTCB grants access to. */
    seL4_Error error = seL4_TCB_ReadRegisters(seL4_CapInitThreadTCB, 0, 0, num_registers, &registers);
    assert(error == seL4_NoError);

    /* set new register values */
    registers.sp = new_sp; // the new stack pointer, derived by prior code.

    /* Write new values */
    error = seL4_TCB_WriteRegisters(seL4_CapInitThreadTCB, 0, 0, num_registers, &registers);
    assert(error == seL4_NoError);
```
**CNodes and CSlots:**

CNode（capability-node）是一个充满能力的对象：你可以把CNode看作是一个能力数组。我们将插槽称为CSlots（能力插槽）。在上面的示例中，seL4_CapInitThreadTCB是根任务的CNode中的插槽，它包含根任务的TCB的功能。CNode中的每个CSlot可以处于以下状态：

- empty: the CNode slot contains a null capability,

- full: the slot contains a capability to a kernel resource.

按照惯例，第0个CSlot保持为空，原因与在进程虚拟地址空间中保持NULL未映射的原因相同：为了避免无意中使用未初始化的插槽时出错。字段info->CNodeSizeBits给出了初始CNode大小的度量：它将有1个<< CNodeSizeBitsCSLots。一个CSlot有1个<< seL4_SlotBits字节，所以一个CNode的字节大小是 1 << (CNodeSizeBits + seL4_SlotBits) 。

**Csapce Addressing:**
要在 capability 上进行相关操作，在之前需要先指定对应的 capability。 seL4 提供了两种指定 capability 的方式： Invocation 和 Direct addressing 。
1. *Invocation:*
- 每个线程有在 TCB 中装载了一个特殊的 CNode 作为它 CSpace 的 root。这个 root 可以为空（代表这个线程没有被赋予任何 capability ）。在 Invocation 方式中，我们通过隐式地调用线程的 CSpace root 来寻址 CSlot 。
例如：我们在seL4_CapInitThreadTCBCSlot上使用调用，以读取和写入由该特定CSlot中的功能表示的TCB的寄存器。这将隐式地在调用线程的CSpace根功能所指向的CNode中查找seL4_CapInitThreadTCBCSlot。
```
seL4_TCB_WriteRegisters(seL4_CapInitThreadTCB, 0, 0, num_registers, &registers);
```
2. *Direct addressing:*
- 直接寻址允许指定要查找的CNode，而不是隐式地使用CSpace根。这种形式的寻址主要用于构造和操作CSpaces的形状--可能是另一个线程的CSpace。直接寻址CS槽时，使用以下参数：
```
_service/root    一种用于操作CNode的能力。
index            要寻址的CNode中的CSlot的索引。
depth            在解析CSlot之前，需要遍历CNode多远。
```
直接寻址根任务的TCB，在CSpace根的第0个插槽中复制它。seL4_CapInitThreadCNode在这里有三个不同的角色：source root、destination root and source slot。
```
seL4_Error error = seL4_CNode_Copy(
        seL4_CapInitThreadCNode, 0, seL4_WordBits, // destination root, slot, and depth
        seL4_CapInitThreadCNode, seL4_CapInitThreadTCB, seL4_WordBits, // source root, slot, and depth
        seL4_AllRights);
    assert(error == seL4_NoError);
```

**Initial CSpace:**
root task 也有一个 CSpace， 在 boot 阶段被设置，包含了被 seL4 管理的所有资源的 capability 。 我们之前用过的控制 TCB 的 seL4_CapInitThreadTCB 和 CSpace root 对应的 seL4_CapInitThreadCNode ，都在 libsel4 中被静态定义为常量，但并不是所有的 capability 都被静态指定，其他 capability 有 seL4_BootInfo 这个数据结构来描述。 seL4_BootInof 描述了初始的所有 capability的范围，包括了初始的 CSpace 中的可用的 Slot
```
typedef struct seL4_BootInfo {
    seL4_Word extraLen; /* length of any additional bootinfo information */
    seL4_NodeId nodeID; /* ID [0..numNodes-1] of the seL4 node (0 if uniprocessor) */
    seL4_Word numNodes; /* number of seL4 nodes (1 if uniprocessor) */
    seL4_Word numIOPTLevels; /* number of IOMMU PT levels (0 if no IOMMU support) */
    seL4_IPCBuffer *ipcBuffer; /* pointer to initial thread's IPC buffer */
    seL4_SlotRegion empty; /* empty slots (null caps) */
    seL4_SlotRegion sharedFrames; /* shared-frame caps (shared between seL4 nodes) */
    seL4_SlotRegion userImageFrames; /* userland-image frame caps */
    seL4_SlotRegion userImagePaging; /* userland-image paging structure caps */
    seL4_SlotRegion ioSpaceCaps; /* IOSpace caps for ARM SMMU */    
    seL4_SlotRegion extraBIPages; /* caps for any pages used to back the additional bootinfo information */
    seL4_Word initThreadCNodeSizeBits; /* initial thread's root CNode size (2^n slots) */
    seL4_Domain initThreadDomain; /* Initial thread's domain ID */
#ifdef CONFIG_KERNEL_MCS
    seL4_SlotRegion schedcontrol; /* Caps to sched_control for each node */
#endif
    seL4_SlotRegion untyped; /* untyped-object caps (untyped caps) */
    seL4_UntypedDesc untypedList[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS]; /* information about each untyped */
    /* the untypedList should be the last entry in this struct, in order
    * to make this struct easier to represent in other languages */
} seL4_BootInfo;
```

## Exercise:
1. **How big is your CSpace?**
参考上面的背景，计算初始线程的CSpace占用的字节数。
```
Answer: 
    // 从默认crt0.S设置的环境变量中解析出seL4_BootInfo数据结构的位置
    seL4_BootInfo *info = platsupport_get_bootinfo();

    // 计算出初始的 cnode 有多少个 slot
    size_t initial_cnode_object_size = BIT(info->initThreadCNodeSizeBits)

    // 计算这些 slot 的总共的大小就是 cnode 的大小
    size_t initial_cnode_object_size_byte = initial_cnode_object_size * (1u << seL4_SlotBits);
```
2. **Copy a capability between CSlots**
```
// 拿到最后一个空闲的 Slot 的位置
seL4_CPtr last_slot = info->empty.end - 1;

// 复制 capability
error = seL4_CNode_Copy(seL4_CapInitThreadCNode, last_slot, seL4_WordBits,
                        seL4_CapInitThreadCNode, seL4_CapInitThreadTCB, seL4_WordBits, seL4_AllRights);
```
3. **How do you delete capabilities?**
```
seL4_CNode_Revoke(seL4_CNode _service, seL4_Word index, seL4_Uint8 depth)
seL4_CNode_Delete(seL4_CNode _service, seL4_Word index, seL4_Uint8 depth)
```
4. **Invoking capabilities**
```
seL4_TCB_Suspend(seL4_CapInitThreadTCB);
```

## build & run
```
# For instructions about obtaining the tutorial sources see https://docs.sel4.systems/Tutorials/#get-the-code
#
# Follow these instructions to initialise the tutorial
# initialising the build directory with a tutorial exercise
./init --tut capabilities
# building the tutorial exercise
cd capabilities_build
ninja
```
最后执行 ./simulate 即可运行程序。