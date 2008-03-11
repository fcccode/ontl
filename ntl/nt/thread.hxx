/**\file*********************************************************************
*                                                                     \brief
*  User Threads
*
****************************************************************************
*/

#ifndef NTL__NT_THREAD
#define NTL__NT_THREAD

#include "basedef.hxx"
#include "object.hxx"
#include "teb.hxx"
#include "apc.hxx"
#include "csr.hxx"


namespace ntl {
  namespace nt {

struct context;
struct initial_tib;

typedef 
ntstatus __stdcall
  control_thread_t(
    const handle* ThreadHandle,
    uint32_t* PreviousSuspendCount
    );

typedef
uint32_t __stdcall 
thread_start_routine_t(
   void* Parameter
   );

NTL__EXTERNAPI
control_thread_t NtResumeThread;

NTL__EXTERNAPI
control_thread_t NtSuspendThread;


enum thread_info_class {
  ThreadBasicInformation,
  ThreadTimes,
  ThreadPriority,
  ThreadBasePriority,
  ThreadAffinityMask,
  ThreadImpersonationToken,
  ThreadDescriptorTableEntry,
  ThreadEnableAlignmentFaultFixup,
  ThreadEventPair,
  ThreadQuerySetWin32StartAddress,
  ThreadZeroTlsCell,
  ThreadPerformanceCount,
  ThreadAmILastThread,
  ThreadIdealProcessor,
  ThreadPriorityBoost,
  ThreadSetTlsArrayAddress,
  MaxThreadInfoClass
};


NTL__EXTERNAPI
ntstatus __stdcall
  NtQueryInformationThread (
    handle* ThreadHandle,
    thread_info_class ThreadInformationClass,
    void* ThreadInformation,
    uint32_t ThreadInformationLength,
    uint32_t* ReturnLength
    );

class user_tread;
  }//namspace nt


  template<>
  struct device_traits<nt::user_tread>: private device_traits<>
  {
    enum access_mask
    {
      terminate                 = 0x0001,
      suspend_resume            = 0x0002,
      alert                     = 0x0004,
      get_context               = 0x0008,
      set_context               = 0x0010,
      set_information           = 0x0020,
      set_limited_information   = 0x0400,
      query_limited_information = 0x0800,
#if 0//(NTDDI_VERSION >= NTDDI_LONGHORN)
      all_access                = standard_rights_required | synchronize | 0xFFFF,
#else                                   
      all_access                = standard_rights_required | synchronize | 0x3FF,
#endif
    };
  };

namespace nt{

NTL__EXTERNAPI
ntstatus __stdcall
  NtCreateThread(
    legacy_handle*  ThreadHandle,
    uint32_t     DesiredAccess,
    object_attributes* ObjectAttributes,
    legacy_handle   ProcessHandle,
    client_id*      ClientId,
    context*        ThreadContext,
    initial_tib*     UserStack,
    bool            CreateSuspended
  );

NTL__EXTERNAPI
ntstatus __stdcall
  RtlCreateUserThread(
    legacy_handle Process,
    security_descriptor* ThreadSecurityDescriptor,
    bool CreateSuspended,
    uint32_t ZeroBits,
    size_t MaximumStackSize,
    size_t CommittedStackSize,
    thread_start_routine_t* StartAddress,
    void* Parameter,
    handle* Thread,
    client_id* ClientId
    );

NTL__EXTERNAPI
ntstatus __stdcall
  NtOpenThread(
    handle* ThreadHandle,
    uint32_t DesiredAccess,
    object_attributes* ObjectAttributes,
    client_id* ClientId
    );

NTL__EXTERNAPI
ntstatus __stdcall
  NtTerminateThread(
    handle* ThreadHandle,
    ntstatus ExitStatus
    );

NTL__EXTERNAPI
ntstatus __stdcall
  NtQueueApcThread(
    handle* ThreadHandle,
    knormal_routine_t* ApcRoutine,
    const void *  ApcContext,
    const void *  Argument1,
    const void *  Argument2
    );

NTL__EXTERNAPI
ntstatus __stdcall
  NtTerminateProcess(
    legacy_handle ProcessHandle,
    ntstatus ExitStatus
    );



NTL__EXTERNAPI
ntstatus __stdcall
  RtlAcquirePebLock();

NTL__EXTERNAPI
ntstatus __stdcall
  RtlReleasePebLock();

NTL__EXTERNAPI
void __stdcall
  RtlExitUserThread(ntstatus Status);

NTL__EXTERNAPI
void __stdcall
  RtlRaiseStatus(ntstatus Status);


NTL__EXTERNAPI
ntstatus __stdcall
  LdrShutdownProcess();

NTL__EXTERNAPI
ntstatus __stdcall
  LdrShutdownThread();


/************************************************************************/
/* user_thread                                                          */
/************************************************************************/
class user_tread:
  public handle, 
  public device_traits<user_tread>
{
  ////////////////////////////////////////////////////////////////////////////
public:

  typedef thread_start_routine_t start_routine_t;

  explicit
    user_tread(
    start_routine_t *   start_routine,
    void *              start_context   = 0,
    size_t              maximum_stack_size = 0,
    size_t              commited_stack_size = 0,
    bool                create_suspended = false,
    client_id *         client          = 0,
    security_descriptor* thread_security = 0) 
  {
    create(this, current_process(), start_routine, start_context, maximum_stack_size, commited_stack_size,
      create_suspended, client, thread_security);
  }

  user_tread(
    access_mask     DesiredAccess,
    client_id*      ClientId,
    bool            InheritHandle = false)
 {
    open(this, DesiredAccess, InheritHandle, ClientId);
  }

  static
    ntstatus
    create(
    handle *            thread_handle,
    legacy_handle       process_handle,
    start_routine_t *   start_routine,
    void *              start_context   = 0,
    size_t              maximum_stack_size = 0,
    size_t              commited_stack_size = 0,
    bool                create_suspended = false,
    client_id *         client          = 0,
    security_descriptor* thread_security = 0)
  {
    return RtlCreateUserThread(process_handle, thread_security, create_suspended, 0, 
      maximum_stack_size, commited_stack_size, start_routine, start_context, thread_handle, client);
  }

  static ntstatus
    open(
    handle*  ThreadHandle,
    access_mask     DesiredAccess,
    bool            InheritHandle,
    client_id*      ClientId)
  {
    object_attributes oa(InheritHandle ? object_attributes::inherit : object_attributes::none);
    return NtOpenThread(ThreadHandle, DesiredAccess, &oa, ClientId);
  }

  ntstatus control(bool suspend)
  {
    return (suspend ? NtSuspendThread : NtResumeThread)(this, NULL);
  }

  void exit(ntstatus st)
  {
    RtlExitUserThread(st);
  }

  void exit_process(ntstatus st)
  {
    RtlAcquirePebLock();
    __try{
      NtTerminateProcess(NULL, st);
      LdrShutdownProcess();
      base_api_msg msg;
      msg.u.ExitProcess.uExitCode = st;
      CsrClientCallServer((csr_api_msg*) &msg, NULL, MAKE_API_NUMBER(1, BasepExitProcess), (uint32_t)sizeof(base_exitprocess_msg));
      NtTerminateProcess(current_process(), st);
    }
    __finally{
      RtlReleasePebLock();
    }
  }
};

/************************************************************************/
/* CONTEXT                                                              */
/************************************************************************/

#if defined(_M_IX86)

struct floating_save_area {
  uint32_t   ControlWord;
  uint32_t   StatusWord;
  uint32_t   TagWord;
  uint32_t   ErrorOffset;
  uint32_t   ErrorSelector;
  uint32_t   DataOffset;
  uint32_t   DataSelector;
  uint8_t    RegisterArea[80];
  uint32_t   Cr0NpxState;
};

struct context {
  uint32_t ContextFlags;

  uint32_t   Dr0;
  uint32_t   Dr1;
  uint32_t   Dr2;
  uint32_t   Dr3;
  uint32_t   Dr6;
  uint32_t   Dr7;

  floating_save_area FloatSave;

  uint32_t   SegGs;
  uint32_t   SegFs;
  uint32_t   SegEs;
  uint32_t   SegDs;

  uint32_t   Edi;
  uint32_t   Esi;
  uint32_t   Ebx;
  uint32_t   Edx;
  uint32_t   Ecx;
  uint32_t   Eax;

  uint32_t   Ebp;
  uint32_t   Eip;
  uint32_t   SegCs;
  uint32_t   EFlags;
  uint32_t   Esp;
  uint32_t   SegSs;

  uint8_t   ExtendedRegisters[512];

};

#elif defined(_M_X64)

//
// Define 128-bit 16-byte aligned xmm register type.
//

struct __declspec(align(16)) m128_t
{
  uint64_t Low;
  int64_t High;
};

//
// Format of data for 32-bit fxsave/fxrstor instructions.
//

struct xmm_save_area32 
{
  uint16_t ControlWord;
  uint16_t StatusWord;
  uint8_t TagWord;
  uint8_t Reserved1;
  uint16_t ErrorOpcode;
  uint32_t ErrorOffset;
  uint16_t ErrorSelector;
  uint16_t Reserved2;
  uint32_t DataOffset;
  uint16_t DataSelector;
  uint16_t Reserved3;
  uint32_t MxCsr;
  uint32_t MxCsr_Mask;
  m128_t FloatRegisters[8];
  m128_t XmmRegisters[16];
  uint8_t Reserved4[96];
};

struct __declspec(align(16)) context {

  //
  // Register parameter home addresses.
  //
  // N.B. These fields are for convience - they could be used to extend the
  //      context record in the future.
  //

  uint64_t P1Home;
  uint64_t P2Home;
  uint64_t P3Home;
  uint64_t P4Home;
  uint64_t P5Home;
  uint64_t P6Home;

  //
  // Control flags.
  //

  uint32_t ContextFlags;
  uint32_t MxCsr;

  //
  // Segment Registers and processor flags.
  //

  uint16_t SegCs;
  uint16_t SegDs;
  uint16_t SegEs;
  uint16_t SegFs;
  uint16_t SegGs;
  uint16_t SegSs;
  uint32_t EFlags;

  //
  // Debug registers
  //

  uint64_t Dr0;
  uint64_t Dr1;
  uint64_t Dr2;
  uint64_t Dr3;
  uint64_t Dr6;
  uint64_t Dr7;

  //
  // Integer registers.
  //

  uint64_t Rax;
  uint64_t Rcx;
  uint64_t Rdx;
  uint64_t Rbx;
  uint64_t Rsp;
  uint64_t Rbp;
  uint64_t Rsi;
  uint64_t Rdi;
  uint64_t R8;
  uint64_t R9;
  uint64_t R10;
  uint64_t R11;
  uint64_t R12;
  uint64_t R13;
  uint64_t R14;
  uint64_t R15;

  //
  // Program counter.
  //

  uint64_t Rip;

  //
  // Floating point state.
  //

  union {
    xmm_save_area32 FltSave;
    struct {
      m128_t Header[2];
      m128_t Legacy[8];
      m128_t Xmm0;
      m128_t Xmm1;
      m128_t Xmm2;
      m128_t Xmm3;
      m128_t Xmm4;
      m128_t Xmm5;
      m128_t Xmm6;
      m128_t Xmm7;
      m128_t Xmm8;
      m128_t Xmm9;
      m128_t Xmm10;
      m128_t Xmm11;
      m128_t Xmm12;
      m128_t Xmm13;
      m128_t Xmm14;
      m128_t Xmm15;
    };
  };

  //
  // Vector registers.
  //

  m128_t VectorRegister[26];
  uint64_t VectorControl;

  //
  // Special debug control registers.
  //

  uint64_t DebugControl;
  uint64_t LastBranchToRip;
  uint64_t LastBranchFromRip;
  uint64_t LastExceptionToRip;
  uint64_t LastExceptionFromRip;
};
#endif 

  }
}
#endif