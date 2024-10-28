# sel4 Untyped memory management

## Background
[官方教程](https://docs.sel4.systems/Tutorials/untyped.html)

**Physical memory:**

除了少量的静态内核内存外，所有物理内存都由 seL4 系统中的用户级管理。sel4 中没有内存分配器 allocater, seL4 在 boot 时创建的对象的 Capability 以及由seL4 管理的其余物理资源在启动时传递给 root task。

**Untyped memory and capabilities:**

除了用于构建 root task 的内存外其余所有可用的物理内存的能力都作为 Untyped memory capability 传递给 root task。未类型化内存是具有特定大小的连续物理内存块。未类型化能力可以重新类型化为内核对象以及对它们的能力，或者进一步重新类型化为更小的未类型化能力。untyped capabilities 有一个 bool 的属性，指明对应的内存对象是否可以被内核写：内存对象可能不是 RAM 而是其他 Device，或者它可能位于内核无法寻址到的 RAM 区域。未类型化能力具有一个布尔属性device，该属性表明内存是否可以被内核写入：它可能不是由RAM支持，而是其他某种设备，或者它可能位于内核无法寻址的RAM区域。设备未类型化能力只能被重新类型化为帧对象（物理内存帧，可以映射到虚拟内存中），并且不能被内核写入。

**Initial state**

提供给根任务的seL4_BootInfo结构描述了所有未类型化能力，包括它们的大小、是否为设备未类型化，以及未类型化的物理地址。下面的代码示例展示了如何打印出从seL4_BootInfo提供的初始未类型化能力。
```
 printf("    CSlot   \tPaddr           \tSize\tType\n");
    for (seL4_CPtr slot = info->untyped.start; slot != info->untyped.end; slot++) {
        seL4_UntypedDesc *desc = &info->untypedList[slot - info->untyped.start];
        printf("%8p\t%16p\t2^%d\t%s\n", (void *) slot, (void *) desc->paddr, desc->sizeBits, desc->isDevice ? "device untyped" : "untyped");
    }
```

**Re-typing capabilities**

未类型化能力有一个单一的调用：seL4_Untyped_Retype，它用于从一个未类型化能力创建一个新的能力（和潜在的对象）。具体来说，通过重新类型化调用创建的新能力提供了对原始能力内存范围的一个子集的访问，要么是一个更小的未类型化能力，要么是指向具有特定类型的新对象。通过重新类型化未类型化能力创建的新能力被称为该未类型化能力的子代。

未类型化能力以增量的贪婪方式从被调用的未类型化能力中重新类型化，这一点很重要，以便在seL4系统中获得高效的内存利用。每个未类型化能力维护一个单一的水印，水印之前的地址不可用（已经重新类型化），水印之后的地址是空闲的（尚未重新类型化）。在重新类型化操作中，水印首先移动到正在创建的对象的对齐位置，然后移动到对象大小的末尾。例如，如果我们首先创建一个4KiB对象，然后创建一个16KiB对象，那么4KiB对象结束和16KiB对象开始之间的12KiB由于对齐而浪费。在两个子代都被撤销之前，内存无法回收。

简而言之：为了避免浪费内存，应该按照大小顺序分配对象，首先是最大的。
```
 error = seL4_Untyped_Retype(parent_untyped, // the untyped capability to retype
                                seL4_UntypedObject, // type
                                untyped_size_bits,  //size
                                seL4_CapInitThreadCNode, // root
                                0, // node_index
                                0, // node_depth
                                child_untyped, // node_offset
                                1 // num_caps
                                );
```
上述代码片段展示了一个将未类型化能力重新类型化为一个更小的、4KiB的未类型化能力的例子（尽管名称是seL4_UntypedObject，实际上并没有创建对象，新的未类型化能力只是指向父未类型化能力的未类型化内存的一个子集。然而，为了方便起见，我们将这个内存当作一个对象来处理）。我们将在讨论重新类型化调用的每个参数时引用这个片段。记住，第一个参数是要被调用以执行重新类型化操作的未类型化能力。

**Types**

在seL4中，每个对象都有特定的类型，所有的类型常量都可以在libsel4中找到。有些类型是特定于架构的，而其他类型则在不同架构中通用。在上面的例子中，我们创建了一个新的未类型化能力，这个能力可以进一步被重新类型化。所有其他的对象类型参数都会创建内核对象以及对它们的能力，并且创建的对象（和能力）的类型决定了可以对那个能力进行的调用。例如，如果我们重新类型化为一个线程控制块（TCB - seL4_TCBObject），那么我们就可以在新的TCB对象的能力上执行TCB调用，包括seL4_TCB_SetPriority。

**Size**

size参数决定了新对象的大小。这个参数的含义取决于正在请求的对象类型：

大多数seL4中的对象都是固定大小的，对于这些对象，内核将忽略size参数；
seL4_UntypedObject和seL4_SchedContextObject类型允许可变大小，大小以size_bits指定（稍后会有更多说明）；
seL4_CapTableObject类型也是可变大小的，参数指定的是能力槽的数量。使用的机制与size_bits相同，但是针对的是槽的数量，而不是字节大小。
通常在seL4中，如果大小以位为单位，它们是2的幂。“位”不是指对象占用的位数，而是指描述其内容所需的位宽。一个大小为n位的对象测量为2^n字节。此外，通常在seL4中，对象和内存区域与其大小对齐，即一个n位的对象与2^n字节对齐，或者等价地说，对象的地址的底部n位为0。

对于重新类型化，只需记住参数size_bits意味着对象将测量为2^size_bits字节，对于seL4_CapTableObject，你请求的是2^size_bits个槽（你可以通过取2^size_bits + seL4_SlotBits来计算字节大小）。

**Root, node_index & node_depth**

root、node_index和node_depth参数用于指定要在其中放置新能力的CNode。根据深度参数，CSlot通过调用或直接寻址来定位（有关这些术语的解释，请参阅能力教程）。

在上面的例子中，node_depth设置为0，这意味着使用调用寻址：root参数隐式地使用当前线程的CSpace根在seL4_WordBits深度下查找。因此，示例代码指定了根任务的CNode（seL4_CapInitThreadCNode）。在这种情况下，忽略了node_index参数。

如果node_depth值不为0，则使用当前线程的CSpace根作为根进行直接寻址。然后使用node_index参数在指定的node_depth下定位CNode能力，以放置新能力。这是为管理多级CSpaces而设计的，本教程不涵盖此内容。

**Node_offset**

node_offset是在由前几个参数选择的CNode中开始创建新能力的CSlot。在这种情况下，选择了初始CNode中的第一个空CSlot。

**Num_caps**

retype调用可以用来一次创建多个能力和对象——使用这个参数指定能力的数量。注意，这个值有两个限制：

未类型化必须足够大，以容纳所有被重新类型化的内存（num_caps * (1u << size_bits)）。
CNode必须有足够的连续空闲CSlots来容纳所有新的能力。

## Exercise:
1. 计算 child untyped 所需的大小，以便可以使用 child untyped 来创建对象数组中列出的所有对象。记住，对象的大小是以位为单位的，即以2的幂次方来衡量。
```seL4_Word untyped_size_bits = seL4_TCBBits + 1; // 2^11 + 2^4 + 2^5  & 考虑对齐 +1

    //找出一个 untyped 能力，其大小刚好可以容纳 TCB 和 TCB 的能力。
    for (int i = 0; i < (info->untyped.end - info->untyped.start); i++) {
            if (info->untypedList[i].sizeBits >= untyped_size_bits && !info->untypedList[i].isDevice) {
                parent_untyped = info->untyped.start + i;
                break;
            }
        }

    // 创建一个 untyped capability 放到空的 CSlot 中
    error = seL4_Untyped_Retype(parent_untyped, // the untyped capability to retype
                                    seL4_UntypedObject, // type
                                    untyped_size_bits,  //size
                                    seL4_CapInitThreadCNode, // root
                                    0, // node_index
                                    0, // node_depth
                                    child_untyped, // node_offset
                                    1 // num_caps
                                    );

```
2. Create a TCB 、ENDPOINT、NOTIFIACTION Object
从上面的 child_untyped 中创建一个 TCB 对象，并把它的 capability 放到 一个空的 CSlot 中。
```
// use the slot after child_untyped for the new TCB cap:
    seL4_CPtr child_tcb = child_untyped + 1;
    /* TODO create a TCB in CSlot child_tcb */
    error = seL4_Untyped_Retype(child_untyped, 
                                seL4_TCBObject, 
                                0, 
                                seL4_CapInitThreadCNode, 
                                0, 
                                0, 
                                child_tcb, 
                                1
                                );
    ZF_LOGF_IF(error != seL4_NoError, "Failed to retype");
    // try to set the TCB priority
    error = seL4_TCB_SetPriority(child_tcb, seL4_CapInitThreadTCB, 10);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to set priority");
```

3. Delete the objects
在实际应用程序中可能需要删除之前创建的对象，释放内存来创建新的对象，使用 revoke 删除之前创建的对象。
seL4_CNode_Revoke 接口可以清空从某个 untyped 派生出去的所有对象：
```
// 删除从child_untyped派生出去的对象
error = seL4_CNode_Revoke(seL4_CapInitThreadCNode, child_untyped, seL4_WordBits);
assert(error == seL4_NoError);
```
计算这块 untyped 可以生成多少个 endpoint 对象，只需要做大小的除法，在 size_bits 表示方式中，要做幂的减法.

```
// 计算出这个大小可以创建多少个 endpoint 对象
seL4_Word num_eps = BIT(untyped_size_bits - seL4_EndpointBits);

// 之前的revoke操作删除了对象的同时也清空了对应的Slot
error = seL4_Untyped_Retype(child_untyped, seL4_EndpointObject, 0, seL4_CapInitThreadCNode, 0, 0, child_tcb, num_eps);
ZF_LOGF_IF(error != seL4_NoError, "Failed to create endpoints.");
```


## build & run
```
# For instructions about obtaining the tutorial sources see https://docs.sel4.systems/Tutorials/#get-the-code
#
# Follow these instructions to initialise the tutorial
# initialising the build directory with a tutorial exercise
./init --tut untyped
# building the tutorial exercise
cd untyped_build
ninja
```
最后执行 ./simulate 即可运行程序。