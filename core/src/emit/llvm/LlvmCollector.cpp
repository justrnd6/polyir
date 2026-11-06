// The garbage collector's runtime, as the LLVM IR every module that allocates is given (docs/memory.md, Parts 2 and 3). The collector is written
// in C (runtime/Collector.c, which explains how it works, incremental marking and the write barrier included); runtime/gen_collector_ir.sh compiles
// it and writes its IR and the library functions it calls into this file, between the GENERATED and DECLARATIONS markers. With
// CompileOptions::collectGarbage off (a wasm build) a module gets the same allocation functions over plain malloc and free instead, a write barrier
// that does nothing, and no frame records or safe points.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    const char* const kCollector = R"IR(; BEGIN GENERATED
%struct.GcClass = type { ptr, ptr, ptr, i32, i32 }
%struct.GcSet = type { ptr, i64, i64 }
%struct.GcWorklist = type { ptr, i64, i64 }
%struct.GcSweep = type { i32, ptr, ptr }
%struct.GcPauses = type { i64, i64, i64, [1992 x i32] }
%struct.gc_timespec = type { i64, i64 }
%struct.GcGrey = type { i64, i64 }
@gc_top = linkonce_odr global ptr null
@gc_marking = linkonce_odr global i32 0
@gc_state = linkonce_odr global { i32, i32, i32, i32, i32, i32, i32, [4 x i8], [2 x [24 x %struct.GcClass]], [129 x i8], [7 x i8], %struct.GcSet, ptr, i64, i64, %struct.GcSet, ptr, i64, i64, i64, i64, i64, i64, i64, ptr, i64, %struct.GcWorklist, i64, i64, i64, i32, [4 x i8], %struct.GcSweep, %struct.GcPauses, i64, i64, i64, i64 } zeroinitializer
@.gc_str = private constant [110 x i8] c"gc verify: %llu reachable objects were left unmarked by the incremental marking (a write barrier is missing)\0A\00"
@.gc_str.1 = private constant [60 x i8] c"out of memory: %llu bytes are still reachable (limit %llu)\0A\00"
@.gc_str.2 = private constant [47 x i8] c"out of memory: the system has no more to give\0A\00"
@.gc_str.3 = private constant [17 x i8] c"POLYIR_GC_STRESS\00"
@.gc_str.4 = private constant [12 x i8] c"incremental\00"
@.gc_str.5 = private constant [15 x i8] c"POLYIR_GC_MODE\00"
@.gc_str.6 = private constant [5 x i8] c"stop\00"
@.gc_str.7 = private constant [17 x i8] c"POLYIR_GC_REPORT\00"
@.gc_str.8 = private constant [18 x i8] c"POLYIR_GC_CEILING\00"
@.gc_str.9 = private constant [16 x i8] c"POLYIR_GC_SLICE\00"
@.gc.kClassBytes = internal constant [24 x i32] [i32 16, i32 32, i32 48, i32 64, i32 80, i32 96, i32 112, i32 128, i32 160, i32 192, i32 224, i32 256, i32 320, i32 384, i32 448, i32 512, i32 640, i32 768, i32 896, i32 1024, i32 1280, i32 1536, i32 1792, i32 2048]
@.gc_str.10 = private constant [16 x i8] c"gc: %s; %s, %s\0A\00"
@.gc_str.11 = private constant [15 x i8] c"stop-the-world\00"
@.gc_str.12 = private constant [135 x i8] c"%llu collections, %llu objects (%llu bytes) reclaimed, peak %llu bytes, %llu bytes in %llu objects at the end (%llu pages, %llu large)\00"
@.gc_str.13 = private constant [63 x i8] c"%llu pauses, longest %s us, p50 %s us, p99 %s us, %s us in all\00"
@.gc_str.14 = private constant [10 x i8] c"%llu.%llu\00"
define linkonce_odr void @gc_poll() {
  %1 = alloca %struct.gc_timespec
  %2 = alloca %struct.gc_timespec
  %3 = load i32, ptr @gc_state
  %4 = icmp eq i32 %3, 0
  br i1 %4, label %132, label %5
5:                                                ; preds = %0
  %6 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %7 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1816)
  %8 = icmp ugt i64 %6, %7
  br i1 %8, label %9, label %132
9:                                                ; preds = %5
  %10 = call i32 @timespec_get(ptr %2, i32 1)
  %11 = load i64, ptr %2
  %12 = getelementptr i8, ptr %2, i64 8
  %13 = load i64, ptr %12
  %14 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %15 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9952)
  %16 = icmp ugt i64 %14, %15
  br i1 %16, label %17, label %18
17:                                               ; preds = %9
  store i64 %14, ptr getelementptr (i8, ptr @gc_state, i64 9952)
  br label %18
18:                                               ; preds = %17, %9
  %19 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %20 = icmp eq i32 %19, 0
  br i1 %20, label %21, label %57
21:                                               ; preds = %18
  %22 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9928)
  %23 = add i64 %22, 1
  store i64 %23, ptr getelementptr (i8, ptr @gc_state, i64 9928)
  store i32 1, ptr getelementptr (i8, ptr @gc_state, i64 4)
  store i32 1, ptr @gc_marking
  store i64 %14, ptr getelementptr (i8, ptr @gc_state, i64 1888)
  store i32 0, ptr getelementptr (i8, ptr @gc_state, i64 1904)
  %24 = load ptr, ptr @gc_top
  %25 = icmp eq ptr %24, null
  br i1 %25, label %36, label %29
26:                                               ; preds = %39, %29
  %27 = load ptr, ptr %30
  %28 = icmp eq ptr %27, null
  br i1 %28, label %36, label %29
29:                                               ; preds = %21, %26
  %30 = phi ptr [ %27, %26 ], [ %24, %21 ]
  %31 = getelementptr i8, ptr %30, i64 8
  %32 = load i64, ptr %31
  %33 = icmp sgt i64 %32, 0
  br i1 %33, label %34, label %26
34:                                               ; preds = %29
  %35 = getelementptr i8, ptr %30, i64 16
  br label %39
36:                                               ; preds = %26, %21
  %37 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1848)
  %38 = icmp sgt i64 %37, 0
  br i1 %38, label %47, label %57
39:                                               ; preds = %39, %34
  %40 = phi i64 [ 0, %34 ], [ %44, %39 ]
  %41 = getelementptr [0 x ptr], ptr %35, i64 0, i64 %40
  %42 = load ptr, ptr %41
  %43 = ptrtoint ptr %42 to i64
  call fastcc void @.gc.markWord(i64 %43)
  %44 = add i64 %40, 1
  %45 = load i64, ptr %31
  %46 = icmp slt i64 %44, %45
  br i1 %46, label %39, label %26
47:                                               ; preds = %36, %47
  %48 = phi i64 [ %54, %47 ], [ 0, %36 ]
  %49 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1840)
  %50 = getelementptr ptr, ptr %49, i64 %48
  %51 = load ptr, ptr %50
  %52 = load ptr, ptr %51
  %53 = ptrtoint ptr %52 to i64
  call fastcc void @.gc.markWord(i64 %53)
  %54 = add i64 %48, 1
  %55 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1848)
  %56 = icmp slt i64 %54, %55
  br i1 %56, label %47, label %57
57:                                               ; preds = %47, %36, %18
  %58 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 8)
  %59 = icmp ne i32 %58, 0
  %60 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 12)
  %61 = icmp eq i32 %60, 0
  %62 = select i1 %59, i1 %61, i1 false
  %63 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %64 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1832)
  %65 = icmp ule i64 %63, %64
  %66 = select i1 %62, i1 %65, i1 false
  %67 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %68 = icmp eq i32 %67, 1
  br i1 %66, label %79, label %69
69:                                               ; preds = %57
  br i1 %68, label %73, label %70
70:                                               ; preds = %73, %69
  %71 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %72 = icmp eq i32 %71, 2
  br i1 %72, label %76, label %83
73:                                               ; preds = %69, %73
  call fastcc void @.gc.markASlice(i64 -1)
  %74 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %75 = icmp eq i32 %74, 1
  br i1 %75, label %73, label %70
76:                                               ; preds = %70, %76
  call fastcc void @.gc.sweepASlice(i64 -1)
  %77 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %78 = icmp eq i32 %77, 2
  br i1 %78, label %76, label %83
79:                                               ; preds = %57
  %80 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1880)
  br i1 %68, label %81, label %82
81:                                               ; preds = %79
  call fastcc void @.gc.markASlice(i64 %80)
  br label %83
82:                                               ; preds = %79
  call fastcc void @.gc.sweepASlice(i64 %80)
  br label %83
83:                                               ; preds = %76, %82, %81, %70
  %84 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %85 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %86 = icmp eq i32 %85, 0
  br i1 %86, label %87, label %89
87:                                               ; preds = %83
  %88 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1824)
  br label %100
89:                                               ; preds = %83
  %90 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1888)
  %91 = lshr i64 %90, 1
  %92 = add i64 %91, %90
  %93 = icmp ugt i64 %84, %92
  %94 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 16)
  %95 = icmp ne i32 %94, 0
  %96 = select i1 %95, i1 true, i1 %93
  br i1 %96, label %100, label %97
97:                                               ; preds = %89
  %98 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1880)
  %99 = add i64 %98, %84
  br label %100
100:                                              ; preds = %89, %87, %97
  %101 = phi i64 [ %88, %87 ], [ %99, %97 ], [ 0, %89 ]
  store i64 %101, ptr getelementptr (i8, ptr @gc_state, i64 1816)
  %102 = call i32 @timespec_get(ptr %1, i32 1)
  %103 = load i64, ptr %1
  %104 = getelementptr i8, ptr %1, i64 8
  %105 = load i64, ptr %104
  %106 = sub i64 %103, %11
  %107 = mul i64 %106, 1000000000
  %108 = sub i64 %105, %13
  %109 = add i64 %108, %107
  %110 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1936)
  %111 = add i64 %110, 1
  store i64 %111, ptr getelementptr (i8, ptr @gc_state, i64 1936)
  %112 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1952)
  %113 = add i64 %112, %109
  store i64 %113, ptr getelementptr (i8, ptr @gc_state, i64 1952)
  %114 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  %115 = icmp ugt i64 %109, %114
  br i1 %115, label %116, label %117
116:                                              ; preds = %100
  store i64 %109, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  br label %117
117:                                              ; preds = %116, %100
  %118 = udiv i64 %109, 1000
  %119 = icmp ult i64 %109, 1000000
  br i1 %119, label %126, label %120
120:                                              ; preds = %117
  %121 = icmp ult i64 %109, 100000000
  br i1 %121, label %122, label %126
122:                                              ; preds = %120
  %123 = add i64 %118, -1000
  %124 = udiv i64 %123, 100
  %125 = add i64 %124, 1000
  br label %126
126:                                              ; preds = %117, %120, %122
  %127 = phi i64 [ %125, %122 ], [ 1991, %120 ], [ %118, %117 ]
  %128 = and i64 %127, 4294967295
  %129 = getelementptr [1992 x i32], ptr getelementptr (i8, ptr @gc_state, i64 1960), i64 0, i64 %128
  %130 = load i32, ptr %129
  %131 = add i32 %130, 1
  store i32 %131, ptr %129
  br label %132
132:                                              ; preds = %0, %5, %126
  ret void
}
define linkonce_odr void @gc_write_barrier(ptr %0) {
  %2 = load i32, ptr @gc_marking
  %3 = icmp eq i32 %2, 0
  br i1 %3, label %7, label %4
4:                                                ; preds = %1
  %5 = load ptr, ptr %0
  %6 = ptrtoint ptr %5 to i64
  tail call fastcc void @.gc.markWord(i64 %6)
  br label %7
7:                                                ; preds = %4, %1
  ret void
}
define internal fastcc void @.gc.markWord(i64 %0) {
  %2 = icmp eq i64 %0, 0
  %3 = and i64 %0, 15
  %4 = icmp ne i64 %3, 0
  %5 = or i1 %2, %4
  %6 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %7 = icmp ult i64 %0, %6
  %8 = select i1 %5, i1 true, i1 %7
  %9 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %10 = icmp ugt i64 %0, %9
  %11 = select i1 %8, i1 true, i1 %10
  br i1 %11, label %172, label %12
12:                                               ; preds = %1
  %13 = and i64 %0, -65536
  %14 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1712)
  %15 = icmp eq i64 %14, 0
  br i1 %15, label %44, label %16
16:                                               ; preds = %12
  %17 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1704)
  %18 = lshr i64 %13, 4
  %19 = mul i64 %18, -7046029254386353131
  %20 = lshr i64 %19, 20
  %21 = add i64 %14, 17592186044415
  %22 = and i64 %21, %20
  %23 = getelementptr i64, ptr %17, i64 %22
  %24 = load i64, ptr %23
  %25 = icmp eq i64 %24, 0
  %26 = icmp eq i64 %24, %13
  %27 = or i1 %25, %26
  br i1 %27, label %39, label %28
28:                                               ; preds = %16
  %29 = add i64 %14, -1
  br label %30
30:                                               ; preds = %30, %28
  %31 = phi i64 [ %22, %28 ], [ %33, %30 ]
  %32 = add i64 %31, 1
  %33 = and i64 %32, %29
  %34 = getelementptr i64, ptr %17, i64 %33
  %35 = load i64, ptr %34
  %36 = icmp eq i64 %35, 0
  %37 = icmp eq i64 %35, %13
  %38 = or i1 %36, %37
  br i1 %38, label %39, label %30
39:                                               ; preds = %30, %16
  %40 = phi i64 [ %22, %16 ], [ %33, %30 ]
  %41 = getelementptr i64, ptr %17, i64 %40
  %42 = load i64, ptr %41
  %43 = icmp ne i64 %42, %13
  br label %44
44:                                               ; preds = %12, %39
  %45 = phi i1 [ true, %12 ], [ %43, %39 ]
  %46 = inttoptr i64 %13 to ptr
  %47 = icmp eq i64 %13, 0
  %48 = or i1 %47, %45
  br i1 %48, label %108, label %49
49:                                               ; preds = %44
  %50 = or i64 %13, 1072
  %51 = icmp ult i64 %0, %50
  br i1 %51, label %77, label %52
52:                                               ; preds = %49
  %53 = sub i64 %0, %50
  %54 = getelementptr i8, ptr %46, i64 8
  %55 = load i32, ptr %54
  %56 = zext i32 %55 to i64
  %57 = freeze i64 %56
  %58 = udiv i64 %53, %57
  %59 = mul i64 %58, %57
  %60 = sub i64 %53, %59
  %61 = icmp eq i64 %60, 0
  br i1 %61, label %62, label %77
62:                                               ; preds = %52
  %63 = getelementptr i8, ptr %46, i64 12
  %64 = load i32, ptr %63
  %65 = zext i32 %64 to i64
  %66 = icmp ult i64 %58, %65
  br i1 %66, label %67, label %77
67:                                               ; preds = %62
  %68 = getelementptr i8, ptr %46, i64 40
  %69 = lshr i64 %58, 6
  %70 = getelementptr i64, ptr %68, i64 %69
  %71 = load i64, ptr %70
  %72 = and i64 %58, 63
  %73 = shl i64 1, %72
  %74 = and i64 %71, %73
  %75 = icmp eq i64 %74, 0
  %76 = select i1 %75, i64 -1, i64 %58
  br label %77
77:                                               ; preds = %49, %52, %62, %67
  %78 = phi i64 [ -1, %52 ], [ -1, %49 ], [ -1, %62 ], [ %76, %67 ]
  %79 = icmp slt i64 %78, 0
  br i1 %79, label %172, label %80
80:                                               ; preds = %77
  %81 = getelementptr i8, ptr %46, i64 552
  %82 = lshr i64 %78, 6
  %83 = getelementptr i64, ptr %81, i64 %82
  %84 = load i64, ptr %83
  %85 = and i64 %78, 63
  %86 = shl i64 1, %85
  %87 = and i64 %84, %86
  %88 = icmp eq i64 %87, 0
  br i1 %88, label %89, label %172
89:                                               ; preds = %80
  %90 = or i64 %84, %86
  store i64 %90, ptr %83
  %91 = getelementptr i8, ptr %46, i64 16
  %92 = load i32, ptr %91
  %93 = icmp eq i32 %92, 1
  br i1 %93, label %94, label %172
94:                                               ; preds = %89
  %95 = or i64 %0, 1
  %96 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %97 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1872)
  %98 = icmp ult i64 %96, %97
  br i1 %98, label %165, label %99
99:                                               ; preds = %94
  %100 = icmp eq i64 %97, 0
  %101 = shl i64 %97, 1
  %102 = select i1 %100, i64 256, i64 %101
  store i64 %102, ptr getelementptr (i8, ptr @gc_state, i64 1872)
  %103 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  %104 = shl i64 %102, 4
  %105 = tail call ptr @realloc(ptr %103, i64 %104)
  %106 = icmp eq ptr %105, null
  br i1 %106, label %107, label %162
107:                                              ; preds = %99
  call void @.gc.markWord.cold.1()
  unreachable
108:                                              ; preds = %44
  %109 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1760)
  %110 = icmp eq i64 %109, 0
  br i1 %110, label %172, label %111
111:                                              ; preds = %108
  %112 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1752)
  %113 = lshr i64 %0, 4
  %114 = mul i64 %113, -7046029254386353131
  %115 = lshr i64 %114, 20
  %116 = add i64 %109, 17592186044415
  %117 = and i64 %116, %115
  %118 = getelementptr i64, ptr %112, i64 %117
  %119 = load i64, ptr %118
  %120 = icmp eq i64 %119, 0
  %121 = icmp eq i64 %119, %0
  %122 = or i1 %120, %121
  br i1 %122, label %134, label %123
123:                                              ; preds = %111
  %124 = add i64 %109, -1
  br label %125
125:                                              ; preds = %125, %123
  %126 = phi i64 [ %117, %123 ], [ %128, %125 ]
  %127 = add i64 %126, 1
  %128 = and i64 %127, %124
  %129 = getelementptr i64, ptr %112, i64 %128
  %130 = load i64, ptr %129
  %131 = icmp eq i64 %130, 0
  %132 = icmp eq i64 %130, %0
  %133 = or i1 %131, %132
  br i1 %133, label %134, label %125
134:                                              ; preds = %125, %111
  %135 = phi i64 [ %117, %111 ], [ %128, %125 ]
  %136 = getelementptr i64, ptr %112, i64 %135
  %137 = load i64, ptr %136
  %138 = icmp eq i64 %137, %0
  br i1 %138, label %139, label %172
139:                                              ; preds = %134
  %140 = add i64 %0, -32
  %141 = inttoptr i64 %140 to ptr
  %142 = getelementptr i8, ptr %141, i64 28
  %143 = load i32, ptr %142
  %144 = icmp eq i32 %143, 0
  br i1 %144, label %145, label %172
145:                                              ; preds = %139
  store i32 1, ptr %142
  %146 = getelementptr i8, ptr %141, i64 24
  %147 = load i32, ptr %146
  %148 = icmp eq i32 %147, 1
  br i1 %148, label %149, label %172
149:                                              ; preds = %145
  %150 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %151 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1872)
  %152 = icmp ult i64 %150, %151
  br i1 %152, label %165, label %153
153:                                              ; preds = %149
  %154 = icmp eq i64 %151, 0
  %155 = shl i64 %151, 1
  %156 = select i1 %154, i64 256, i64 %155
  store i64 %156, ptr getelementptr (i8, ptr @gc_state, i64 1872)
  %157 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  %158 = shl i64 %156, 4
  %159 = tail call ptr @realloc(ptr %157, i64 %158)
  %160 = icmp eq ptr %159, null
  br i1 %160, label %161, label %162
161:                                              ; preds = %153
  call void @.gc.markWord.cold.2()
  unreachable
162:                                              ; preds = %153, %99
  %163 = phi ptr [ %105, %99 ], [ %159, %153 ]
  %164 = phi i64 [ %95, %99 ], [ %0, %153 ]
  store ptr %163, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  br label %165
165:                                              ; preds = %162, %149, %94
  %166 = phi i64 [ %95, %94 ], [ %0, %149 ], [ %164, %162 ]
  %167 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  %168 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %169 = add i64 %168, 1
  store i64 %169, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %170 = getelementptr %struct.GcGrey, ptr %167, i64 %168
  store i64 %166, ptr %170
  %171 = getelementptr i8, ptr %170, i64 8
  store i64 0, ptr %171
  br label %172
172:                                              ; preds = %165, %108, %145, %139, %89, %80, %77, %134, %1
  ret void
}
define linkonce_odr ptr @gc_allocate(i64 %0, i64 %1) {
  %3 = tail call i64 @llvm.smax.i64(i64 %0, i64 1)
  %4 = load i32, ptr @gc_state
  %5 = icmp eq i32 %4, 0
  br i1 %5, label %6, label %10
6:                                                ; preds = %2
  %7 = tail call ptr @malloc(i64 %3)
  %8 = icmp eq ptr %7, null
  br i1 %8, label %9, label %12
9:                                                ; preds = %6
  call void @.gc.gc_allocate.cold.1()
  unreachable
10:                                               ; preds = %2
  %11 = tail call fastcc ptr @.gc.newObject(i64 %3, i64 %1)
  br label %12
12:                                               ; preds = %6, %10
  %13 = phi ptr [ %11, %10 ], [ %7, %6 ]
  ret ptr %13
}
define internal fastcc ptr @.gc.newObject(i64 %0, i64 %1) {
  %3 = alloca ptr
  %4 = icmp ult i64 %0, 2049
  %5 = icmp eq i64 %1, 1
  br i1 %4, label %6, label %234
6:                                                ; preds = %2
  %7 = zext i1 %5 to i64
  %8 = add i64 %0, 15
  %9 = lshr i64 %8, 4
  %10 = getelementptr [129 x i8], ptr getelementptr (i8, ptr @gc_state, i64 1568), i64 0, i64 %9
  %11 = load i8, ptr %10
  %12 = zext i8 %11 to i64
  %13 = getelementptr [2 x [24 x %struct.GcClass]], ptr getelementptr (i8, ptr @gc_state, i64 32), i64 0, i64 %7, i64 %12
  %14 = getelementptr i8, ptr %13, i64 16
  %15 = load ptr, ptr %14
  %16 = icmp eq ptr %15, null
  br i1 %16, label %83, label %17
17:                                               ; preds = %6
  %18 = getelementptr i8, ptr %15, i64 28
  %19 = getelementptr i8, ptr %15, i64 24
  %20 = load i32, ptr %19
  %21 = load i32, ptr %18
  %22 = icmp ult i32 %21, %20
  br i1 %22, label %23, label %49
23:                                               ; preds = %17
  %24 = getelementptr i8, ptr %15, i64 40
  %25 = getelementptr i8, ptr %15, i64 12
  %26 = zext i32 %21 to i64
  %27 = zext i32 %20 to i64
  br label %28
28:                                               ; preds = %45, %23
  %29 = phi i64 [ %46, %45 ], [ %26, %23 ]
  %30 = phi i64 [ %44, %45 ], [ undef, %23 ]
  %31 = getelementptr [64 x i64], ptr %24, i64 0, i64 %29
  %32 = load i64, ptr %31
  %33 = icmp eq i64 %32, -1
  br i1 %33, label %43, label %34
34:                                               ; preds = %28
  %35 = xor i64 %32, -1
  %36 = shl i64 %29, 6
  %37 = tail call i64 @llvm.cttz.i64(i64 %35, i1 true)
  %38 = or i64 %37, %36
  %39 = load i32, ptr %25
  %40 = zext i32 %39 to i64
  %41 = icmp ult i64 %38, %40
  %42 = select i1 %41, i64 %38, i64 -1
  br label %43
43:                                               ; preds = %34, %28
  %44 = phi i64 [ %42, %34 ], [ %30, %28 ]
  br i1 %33, label %45, label %49
45:                                               ; preds = %43
  %46 = add i64 %29, 1
  %47 = trunc i64 %46 to i32
  store i32 %47, ptr %18
  %48 = icmp eq i64 %46, %27
  br i1 %48, label %49, label %28
49:                                               ; preds = %45, %43, %17
  %50 = phi i64 [ -1, %17 ], [ %44, %43 ], [ -1, %45 ]
  %51 = icmp sgt i64 %50, -1
  br i1 %51, label %52, label %82
52:                                               ; preds = %49
  %53 = getelementptr i8, ptr %15, i64 40
  %54 = and i64 %50, 63
  %55 = shl i64 1, %54
  %56 = lshr i64 %50, 6
  %57 = getelementptr i64, ptr %53, i64 %56
  %58 = load i64, ptr %57
  %59 = or i64 %58, %55
  store i64 %59, ptr %57
  %60 = getelementptr i8, ptr %15, i64 20
  %61 = load i32, ptr %60
  %62 = add i32 %61, 1
  store i32 %62, ptr %60
  %63 = load i32, ptr @gc_marking
  %64 = icmp eq i32 %63, 0
  br i1 %64, label %65, label %70
65:                                               ; preds = %52
  %66 = getelementptr i8, ptr %15, i64 32
  %67 = load i32, ptr %66
  %68 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 24)
  %69 = icmp eq i32 %67, %68
  br i1 %69, label %75, label %70
70:                                               ; preds = %65, %52
  %71 = getelementptr i8, ptr %15, i64 552
  %72 = getelementptr i64, ptr %71, i64 %56
  %73 = load i64, ptr %72
  %74 = or i64 %73, %55
  store i64 %74, ptr %72
  br label %75
75:                                               ; preds = %70, %65
  %76 = getelementptr i8, ptr %15, i64 1072
  %77 = getelementptr i8, ptr %15, i64 8
  %78 = load i32, ptr %77
  %79 = zext i32 %78 to i64
  %80 = mul i64 %50, %79
  %81 = getelementptr i8, ptr %76, i64 %80
  br label %83
82:                                               ; preds = %49
  store i32 %20, ptr %18
  br label %83
83:                                               ; preds = %82, %75, %6
  %84 = phi ptr [ null, %6 ], [ %81, %75 ], [ null, %82 ]
  %85 = icmp eq ptr %84, null
  br i1 %85, label %86, label %222
86:                                               ; preds = %83
  %87 = getelementptr i8, ptr %13, i64 24
  %88 = getelementptr i8, ptr %13, i64 28
  %89 = getelementptr i8, ptr %13, i64 8
  br label %90
90:                                               ; preds = %219, %86
  %91 = load ptr, ptr %14
  %92 = icmp eq ptr %91, null
  %93 = select i1 %92, ptr %13, ptr %91
  br label %94
94:                                               ; preds = %98, %90
  %95 = phi ptr [ %93, %90 ], [ %96, %98 ]
  %96 = load ptr, ptr %95
  %97 = icmp eq ptr %96, null
  br i1 %97, label %104, label %98
98:                                               ; preds = %94
  %99 = getelementptr i8, ptr %96, i64 20
  %100 = load i32, ptr %99
  %101 = getelementptr i8, ptr %96, i64 12
  %102 = load i32, ptr %101
  %103 = icmp eq i32 %100, %102
  br i1 %103, label %94, label %152
104:                                              ; preds = %94
  %105 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  %106 = icmp eq ptr %105, null
  br i1 %106, label %114, label %107
107:                                              ; preds = %104
  %108 = load ptr, ptr %105
  store ptr %108, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  %109 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  %110 = add i64 %109, -1
  store i64 %110, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  %111 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1744)
  %112 = icmp ult i64 %110, %111
  br i1 %112, label %113, label %133
113:                                              ; preds = %107
  store i64 %110, ptr getelementptr (i8, ptr @gc_state, i64 1744)
  br label %133
114:                                              ; preds = %104
  store ptr null, ptr %3
  %115 = call i32 @posix_memalign(ptr %3, i64 65536, i64 65536)
  %116 = icmp eq i32 %115, 0
  br i1 %116, label %118, label %117
117:                                              ; preds = %114
  call void @.gc.newObject.cold.2()
  unreachable
118:                                              ; preds = %114
  %119 = load ptr, ptr %3
  %120 = ptrtoint ptr %119 to i64
  call fastcc void @.gc.setAdd(ptr getelementptr (i8, ptr @gc_state, i64 1704), i64 %120)
  %121 = load ptr, ptr %3
  %122 = ptrtoint ptr %121 to i64
  %123 = add i64 %122, 65535
  %124 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %125 = add i64 %124, -1
  %126 = icmp ult i64 %125, %122
  br i1 %126, label %128, label %127
127:                                              ; preds = %118
  store i64 %122, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  br label %128
128:                                              ; preds = %127, %118
  %129 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %130 = icmp ugt i64 %123, %129
  br i1 %130, label %131, label %132
131:                                              ; preds = %128
  store i64 %123, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  br label %132
132:                                              ; preds = %131, %128
  br label %133
133:                                              ; preds = %132, %113, %107
  %134 = phi ptr [ %121, %132 ], [ %105, %107 ], [ %105, %113 ]
  call void @llvm.memset.p0.i64(ptr %134, i8 0, i64 1064, i1 false)
  %135 = load i32, ptr %87
  %136 = getelementptr i8, ptr %134, i64 8
  store i32 %135, ptr %136
  %137 = load i32, ptr %87
  %138 = udiv i32 64464, %137
  %139 = getelementptr i8, ptr %134, i64 12
  store i32 %138, ptr %139
  %140 = add i32 %138, 63
  %141 = lshr i32 %140, 6
  %142 = getelementptr i8, ptr %134, i64 24
  store i32 %141, ptr %142
  %143 = load i32, ptr %88
  %144 = getelementptr i8, ptr %134, i64 16
  store i32 %143, ptr %144
  %145 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 24)
  %146 = getelementptr i8, ptr %134, i64 32
  store i32 %145, ptr %146
  %147 = load ptr, ptr %89
  %148 = icmp eq ptr %147, null
  br i1 %148, label %150, label %149
149:                                              ; preds = %133
  store ptr %134, ptr %147
  br label %151
150:                                              ; preds = %133
  store ptr %134, ptr %13
  br label %151
151:                                              ; preds = %150, %149
  store ptr %134, ptr %89
  br label %152
152:                                              ; preds = %98, %151
  %153 = phi ptr [ %134, %151 ], [ %96, %98 ]
  store ptr %153, ptr %14
  %154 = getelementptr i8, ptr %153, i64 28
  %155 = getelementptr i8, ptr %153, i64 24
  %156 = load i32, ptr %155
  %157 = load i32, ptr %154
  %158 = icmp ult i32 %157, %156
  br i1 %158, label %159, label %185
159:                                              ; preds = %152
  %160 = getelementptr i8, ptr %153, i64 40
  %161 = getelementptr i8, ptr %153, i64 12
  %162 = zext i32 %157 to i64
  %163 = zext i32 %156 to i64
  br label %164
164:                                              ; preds = %181, %159
  %165 = phi i64 [ %182, %181 ], [ %162, %159 ]
  %166 = phi i64 [ %180, %181 ], [ undef, %159 ]
  %167 = getelementptr [64 x i64], ptr %160, i64 0, i64 %165
  %168 = load i64, ptr %167
  %169 = icmp eq i64 %168, -1
  br i1 %169, label %179, label %170
170:                                              ; preds = %164
  %171 = xor i64 %168, -1
  %172 = shl i64 %165, 6
  %173 = call i64 @llvm.cttz.i64(i64 %171, i1 true)
  %174 = or i64 %173, %172
  %175 = load i32, ptr %161
  %176 = zext i32 %175 to i64
  %177 = icmp ult i64 %174, %176
  %178 = select i1 %177, i64 %174, i64 -1
  br label %179
179:                                              ; preds = %170, %164
  %180 = phi i64 [ %178, %170 ], [ %166, %164 ]
  br i1 %169, label %181, label %185
181:                                              ; preds = %179
  %182 = add i64 %165, 1
  %183 = trunc i64 %182 to i32
  store i32 %183, ptr %154
  %184 = icmp eq i64 %182, %163
  br i1 %184, label %185, label %164
185:                                              ; preds = %181, %179, %152
  %186 = phi i64 [ -1, %152 ], [ %180, %179 ], [ -1, %181 ]
  %187 = icmp sgt i64 %186, -1
  br i1 %187, label %188, label %218
188:                                              ; preds = %185
  %189 = getelementptr i8, ptr %153, i64 40
  %190 = and i64 %186, 63
  %191 = shl i64 1, %190
  %192 = lshr i64 %186, 6
  %193 = getelementptr i64, ptr %189, i64 %192
  %194 = load i64, ptr %193
  %195 = or i64 %194, %191
  store i64 %195, ptr %193
  %196 = getelementptr i8, ptr %153, i64 20
  %197 = load i32, ptr %196
  %198 = add i32 %197, 1
  store i32 %198, ptr %196
  %199 = load i32, ptr @gc_marking
  %200 = icmp eq i32 %199, 0
  br i1 %200, label %201, label %206
201:                                              ; preds = %188
  %202 = getelementptr i8, ptr %153, i64 32
  %203 = load i32, ptr %202
  %204 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 24)
  %205 = icmp eq i32 %203, %204
  br i1 %205, label %211, label %206
206:                                              ; preds = %201, %188
  %207 = getelementptr i8, ptr %153, i64 552
  %208 = getelementptr i64, ptr %207, i64 %192
  %209 = load i64, ptr %208
  %210 = or i64 %209, %191
  store i64 %210, ptr %208
  br label %211
211:                                              ; preds = %206, %201
  %212 = getelementptr i8, ptr %153, i64 1072
  %213 = getelementptr i8, ptr %153, i64 8
  %214 = load i32, ptr %213
  %215 = zext i32 %214 to i64
  %216 = mul i64 %186, %215
  %217 = getelementptr i8, ptr %212, i64 %216
  br label %219
218:                                              ; preds = %185
  store i32 %156, ptr %154
  br label %219
219:                                              ; preds = %218, %211
  %220 = phi ptr [ %217, %211 ], [ null, %218 ]
  %221 = icmp eq ptr %220, null
  br i1 %221, label %90, label %222
222:                                              ; preds = %219, %83
  %223 = phi ptr [ %84, %83 ], [ %220, %219 ]
  br i1 %5, label %224, label %228
224:                                              ; preds = %222
  %225 = getelementptr i8, ptr %13, i64 24
  %226 = load i32, ptr %225
  %227 = zext i32 %226 to i64
  call void @llvm.memset.p0.i64(ptr %223, i8 0, i64 %227, i1 false)
  br label %228
228:                                              ; preds = %222, %224
  %229 = getelementptr i8, ptr %13, i64 24
  %230 = load i32, ptr %229
  %231 = zext i32 %230 to i64
  %232 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %233 = add i64 %232, %231
  br label %270
234:                                              ; preds = %2
  %235 = add i64 %0, 32
  br i1 %5, label %236, label %238
236:                                              ; preds = %234
  %237 = tail call ptr @calloc(i64 1, i64 %235)
  br label %240
238:                                              ; preds = %234
  %239 = tail call ptr @malloc(i64 %235)
  br label %240
240:                                              ; preds = %238, %236
  %241 = phi ptr [ %237, %236 ], [ %239, %238 ]
  %242 = icmp eq ptr %241, null
  br i1 %242, label %243, label %244
243:                                              ; preds = %240
  call void @.gc.newObject.cold.1()
  unreachable
244:                                              ; preds = %240
  %245 = getelementptr i8, ptr %241, i64 16
  store i64 %0, ptr %245
  %246 = trunc i64 %1 to i32
  %247 = getelementptr i8, ptr %241, i64 24
  store i32 %246, ptr %247
  %248 = load i32, ptr @gc_marking
  %249 = getelementptr i8, ptr %241, i64 28
  store i32 %248, ptr %249
  store ptr null, ptr %241
  %250 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  %251 = getelementptr i8, ptr %241, i64 8
  store ptr %250, ptr %251
  %252 = icmp eq ptr %250, null
  br i1 %252, label %254, label %253
253:                                              ; preds = %244
  store ptr %241, ptr %250
  br label %254
254:                                              ; preds = %253, %244
  store ptr %241, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  %255 = getelementptr i8, ptr %241, i64 32
  %256 = ptrtoint ptr %255 to i64
  tail call fastcc void @.gc.setAdd(ptr getelementptr (i8, ptr @gc_state, i64 1752), i64 %256)
  %257 = add i64 %0, %256
  %258 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %259 = add i64 %258, -1
  %260 = icmp ult i64 %259, %256
  br i1 %260, label %262, label %261
261:                                              ; preds = %254
  store i64 %256, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  br label %262
262:                                              ; preds = %261, %254
  %263 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %264 = icmp ugt i64 %257, %263
  br i1 %264, label %265, label %266
265:                                              ; preds = %262
  store i64 %257, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  br label %266
266:                                              ; preds = %262, %265
  %267 = add i64 %0, 32
  %268 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %269 = add i64 %267, %268
  br label %270
270:                                              ; preds = %266, %228
  %271 = phi i64 [ %269, %266 ], [ %233, %228 ]
  %272 = phi ptr [ %255, %266 ], [ %223, %228 ]
  store i64 %271, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %273 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %274 = add i64 %273, 1
  store i64 %274, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  ret ptr %272
}
define linkonce_odr ptr @gc_allocate_zeroed(i64 %0, i64 %1, i64 %2) {
  %4 = icmp sgt i64 %0, 0
  %5 = icmp sgt i64 %1, 0
  %6 = and i1 %4, %5
  %7 = mul i64 %1, %0
  %8 = select i1 %6, i64 %7, i64 1
  %9 = load i32, ptr @gc_state
  %10 = icmp eq i32 %9, 0
  br i1 %10, label %11, label %15
11:                                               ; preds = %3
  %12 = tail call ptr @calloc(i64 1, i64 %8)
  %13 = icmp eq ptr %12, null
  br i1 %13, label %14, label %19
14:                                               ; preds = %11
  call void @.gc.gc_allocate_zeroed.cold.1()
  unreachable
15:                                               ; preds = %3
  %16 = tail call fastcc ptr @.gc.newObject(i64 %8, i64 %2)
  %17 = icmp eq i64 %2, 1
  br i1 %17, label %19, label %18
18:                                               ; preds = %15
  tail call void @llvm.memset.p0.i64(ptr %16, i8 0, i64 %8, i1 false)
  br label %19
19:                                               ; preds = %11, %15, %18
  %20 = phi ptr [ %16, %18 ], [ %16, %15 ], [ %12, %11 ]
  ret ptr %20
}
define linkonce_odr ptr @gc_resize(ptr %0, i64 %1) {
  %3 = tail call i64 @llvm.smax.i64(i64 %1, i64 1)
  %4 = ptrtoint ptr %0 to i64
  %5 = load i32, ptr @gc_state
  %6 = icmp eq i32 %5, 0
  br i1 %6, label %7, label %11
7:                                                ; preds = %2
  %8 = tail call ptr @realloc(ptr %0, i64 %3)
  %9 = icmp eq ptr %8, null
  br i1 %9, label %10, label %251
10:                                               ; preds = %7
  call void @.gc.gc_resize.cold.3()
  unreachable
11:                                               ; preds = %2
  %12 = tail call fastcc ptr @.gc.pageOfObject(i64 %4)
  %13 = icmp eq ptr %12, null
  br i1 %13, label %99, label %14
14:                                               ; preds = %11
  %15 = ptrtoint ptr %12 to i64
  %16 = add i64 %15, 1072
  %17 = icmp ugt i64 %16, %4
  br i1 %17, label %44, label %18
18:                                               ; preds = %14
  %19 = sub i64 %4, %16
  %20 = getelementptr i8, ptr %12, i64 8
  %21 = load i32, ptr %20
  %22 = zext i32 %21 to i64
  %23 = freeze i64 %19
  %24 = freeze i64 %22
  %25 = udiv i64 %23, %24
  %26 = mul i64 %25, %24
  %27 = sub i64 %23, %26
  %28 = icmp eq i64 %27, 0
  br i1 %28, label %29, label %44
29:                                               ; preds = %18
  %30 = getelementptr i8, ptr %12, i64 12
  %31 = load i32, ptr %30
  %32 = zext i32 %31 to i64
  %33 = icmp ult i64 %25, %32
  br i1 %33, label %34, label %44
34:                                               ; preds = %29
  %35 = getelementptr i8, ptr %12, i64 40
  %36 = lshr i64 %25, 6
  %37 = getelementptr i64, ptr %35, i64 %36
  %38 = load i64, ptr %37
  %39 = and i64 %25, 63
  %40 = shl i64 1, %39
  %41 = and i64 %38, %40
  %42 = icmp eq i64 %41, 0
  %43 = select i1 %42, i64 -1, i64 %25
  br label %44
44:                                               ; preds = %14, %18, %29, %34
  %45 = phi i64 [ -1, %18 ], [ -1, %14 ], [ -1, %29 ], [ %43, %34 ]
  %46 = getelementptr i8, ptr %12, i64 8
  %47 = load i32, ptr %46
  %48 = zext i32 %47 to i64
  %49 = icmp ugt i64 %3, %48
  br i1 %49, label %50, label %251
50:                                               ; preds = %44
  %51 = getelementptr i8, ptr %12, i64 16
  %52 = load i32, ptr @gc_marking
  %53 = icmp eq i32 %52, 0
  br i1 %53, label %66, label %54
54:                                               ; preds = %50
  %55 = load i32, ptr %51
  store i32 1, ptr getelementptr (i8, ptr @gc_state, i64 1904)
  %56 = icmp eq i32 %55, 1
  br i1 %56, label %57, label %66
57:                                               ; preds = %54
  %58 = lshr i64 %48, 3
  %59 = icmp ult i32 %47, 8
  br i1 %59, label %66, label %60
60:                                               ; preds = %57, %60
  %61 = phi i64 [ %64, %60 ], [ 0, %57 ]
  %62 = getelementptr i64, ptr %0, i64 %61
  %63 = load i64, ptr %62
  tail call fastcc void @.gc.markWord(i64 %63)
  %64 = add i64 %61, 1
  %65 = icmp eq i64 %64, %58
  br i1 %65, label %66, label %60
66:                                               ; preds = %60, %57, %54, %50
  %67 = load i32, ptr %51
  %68 = zext i32 %67 to i64
  %69 = tail call fastcc ptr @.gc.newObject(i64 %3, i64 %68)
  %70 = load i32, ptr %46
  %71 = zext i32 %70 to i64
  tail call void @llvm.memcpy.p0.p0.i64(ptr %69, ptr %0, i64 %71, i1 false)
  %72 = and i64 %45, 63
  %73 = shl i64 1, %72
  %74 = xor i64 %73, -1
  %75 = getelementptr i8, ptr %12, i64 40
  %76 = sdiv i64 %45, 64
  %77 = getelementptr [64 x i64], ptr %75, i64 0, i64 %76
  %78 = load i64, ptr %77
  %79 = and i64 %78, %74
  store i64 %79, ptr %77
  %80 = getelementptr i8, ptr %12, i64 552
  %81 = getelementptr [64 x i64], ptr %80, i64 0, i64 %76
  %82 = load i64, ptr %81
  %83 = and i64 %82, %74
  store i64 %83, ptr %81
  %84 = getelementptr i8, ptr %12, i64 20
  %85 = load i32, ptr %84
  %86 = add i32 %85, -1
  store i32 %86, ptr %84
  %87 = trunc i64 %76 to i32
  %88 = getelementptr i8, ptr %12, i64 28
  %89 = load i32, ptr %88
  %90 = icmp ugt i32 %89, %87
  br i1 %90, label %91, label %92
91:                                               ; preds = %66
  store i32 %87, ptr %88
  br label %92
92:                                               ; preds = %91, %66
  %93 = load i32, ptr %46
  %94 = zext i32 %93 to i64
  %95 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %96 = sub i64 %95, %94
  store i64 %96, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %97 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %98 = add i64 %97, -1
  store i64 %98, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  br label %251
99:                                               ; preds = %11
  %100 = icmp eq ptr %0, null
  %101 = and i64 %4, 15
  %102 = icmp ne i64 %101, 0
  %103 = or i1 %100, %102
  %104 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %105 = icmp ugt i64 %104, %4
  %106 = select i1 %103, i1 true, i1 %105
  %107 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %108 = icmp ult i64 %107, %4
  %109 = select i1 %106, i1 true, i1 %108
  br i1 %109, label %247, label %110
110:                                              ; preds = %99
  %111 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1760)
  %112 = icmp eq i64 %111, 0
  br i1 %112, label %247, label %113
113:                                              ; preds = %110
  %114 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1752)
  %115 = lshr i64 %4, 4
  %116 = mul i64 %115, -7046029254386353131
  %117 = lshr i64 %116, 20
  %118 = add i64 %111, 17592186044415
  %119 = and i64 %118, %117
  %120 = getelementptr i64, ptr %114, i64 %119
  %121 = load i64, ptr %120
  %122 = icmp eq i64 %121, 0
  %123 = icmp eq i64 %121, %4
  %124 = or i1 %122, %123
  br i1 %124, label %136, label %125
125:                                              ; preds = %113
  %126 = add i64 %111, -1
  br label %127
127:                                              ; preds = %127, %125
  %128 = phi i64 [ %119, %125 ], [ %130, %127 ]
  %129 = add i64 %128, 1
  %130 = and i64 %129, %126
  %131 = getelementptr i64, ptr %114, i64 %130
  %132 = load i64, ptr %131
  %133 = icmp eq i64 %132, 0
  %134 = icmp eq i64 %132, %4
  %135 = or i1 %133, %134
  br i1 %135, label %136, label %127
136:                                              ; preds = %127, %113
  %137 = phi i64 [ %119, %113 ], [ %130, %127 ]
  %138 = getelementptr i64, ptr %114, i64 %137
  %139 = load i64, ptr %138
  %140 = icmp eq i64 %139, %4
  br i1 %140, label %141, label %247
141:                                              ; preds = %136
  %142 = add i64 %4, -32
  %143 = inttoptr i64 %142 to ptr
  %144 = getelementptr i8, ptr %143, i64 16
  %145 = load i64, ptr %144
  %146 = getelementptr i8, ptr %143, i64 24
  %147 = load i32, ptr @gc_marking
  %148 = icmp eq i32 %147, 0
  br i1 %148, label %161, label %149
149:                                              ; preds = %141
  %150 = load i32, ptr %146
  store i32 1, ptr getelementptr (i8, ptr @gc_state, i64 1904)
  %151 = icmp eq i32 %150, 1
  br i1 %151, label %152, label %161
152:                                              ; preds = %149
  %153 = lshr i64 %145, 3
  %154 = icmp ult i64 %145, 8
  br i1 %154, label %161, label %155
155:                                              ; preds = %152, %155
  %156 = phi i64 [ %159, %155 ], [ 0, %152 ]
  %157 = getelementptr i64, ptr %0, i64 %156
  %158 = load i64, ptr %157
  tail call fastcc void @.gc.markWord(i64 %158)
  %159 = add i64 %156, 1
  %160 = icmp eq i64 %159, %153
  br i1 %160, label %161, label %155
161:                                              ; preds = %155, %152, %149, %141
  %162 = icmp sgt i64 %1, 2048
  br i1 %162, label %163, label %218
163:                                              ; preds = %161
  %164 = load i64, ptr %144
  %165 = load i64, ptr %146
  tail call fastcc void @.gc.setRemove(ptr getelementptr (i8, ptr @gc_state, i64 1752), i64 %4)
  %166 = add i64 %3, 32
  %167 = tail call ptr @realloc(ptr %143, i64 %166)
  %168 = icmp eq ptr %167, null
  br i1 %168, label %169, label %170
169:                                              ; preds = %163
  call void @.gc.gc_resize.cold.1()
  unreachable
170:                                              ; preds = %163
  %171 = load ptr, ptr %167
  %172 = icmp eq ptr %171, null
  br i1 %172, label %175, label %173
173:                                              ; preds = %170
  %174 = getelementptr i8, ptr %171, i64 8
  store ptr %167, ptr %174
  br label %176
175:                                              ; preds = %170
  store ptr %167, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  br label %176
176:                                              ; preds = %175, %173
  %177 = getelementptr i8, ptr %167, i64 8
  %178 = load ptr, ptr %177
  %179 = icmp eq ptr %178, null
  br i1 %179, label %181, label %180
180:                                              ; preds = %176
  store ptr %167, ptr %178
  br label %181
181:                                              ; preds = %180, %176
  %182 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %183 = ptrtoint ptr %182 to i64
  %184 = icmp eq i64 %142, %183
  br i1 %184, label %185, label %186
185:                                              ; preds = %181
  store ptr %167, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  br label %186
186:                                              ; preds = %185, %181
  %187 = and i64 %165, 4294967295
  %188 = icmp eq i64 %187, 1
  %189 = icmp ugt i64 %3, %164
  %190 = select i1 %188, i1 %189, i1 false
  br i1 %190, label %191, label %195
191:                                              ; preds = %186
  %192 = getelementptr i8, ptr %167, i64 32
  %193 = getelementptr i8, ptr %192, i64 %164
  %194 = sub i64 %3, %164
  tail call void @llvm.memset.p0.i64(ptr %193, i8 0, i64 %194, i1 false)
  br label %195
195:                                              ; preds = %191, %186
  %196 = getelementptr i8, ptr %167, i64 16
  store i64 %3, ptr %196
  %197 = load i32, ptr @gc_marking
  %198 = icmp eq i32 %197, 0
  %199 = lshr i64 %165, 32
  %200 = trunc i64 %199 to i32
  %201 = select i1 %198, i32 %200, i32 1
  %202 = getelementptr i8, ptr %167, i64 28
  store i32 %201, ptr %202
  %203 = getelementptr i8, ptr %167, i64 32
  %204 = ptrtoint ptr %203 to i64
  tail call fastcc void @.gc.setAdd(ptr getelementptr (i8, ptr @gc_state, i64 1752), i64 %204)
  %205 = add i64 %3, %204
  %206 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %207 = add i64 %206, -1
  %208 = icmp ult i64 %207, %204
  br i1 %208, label %210, label %209
209:                                              ; preds = %195
  store i64 %204, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  br label %210
210:                                              ; preds = %209, %195
  %211 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %212 = icmp ugt i64 %205, %211
  br i1 %212, label %213, label %214
213:                                              ; preds = %210
  store i64 %205, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  br label %214
214:                                              ; preds = %213, %210
  %215 = sub i64 %3, %164
  %216 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %217 = add i64 %215, %216
  store i64 %217, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  br label %251
218:                                              ; preds = %161
  %219 = load i32, ptr %146
  %220 = zext i32 %219 to i64
  %221 = tail call fastcc ptr @.gc.newObject(i64 %3, i64 %220)
  tail call void @llvm.memcpy.p0.p0.i64(ptr %221, ptr %0, i64 %3, i1 false)
  tail call fastcc void @.gc.setRemove(ptr getelementptr (i8, ptr @gc_state, i64 1752), i64 %4)
  %222 = load ptr, ptr %143
  %223 = icmp eq ptr %222, null
  %224 = getelementptr i8, ptr %143, i64 8
  %225 = load ptr, ptr %224
  br i1 %223, label %228, label %226
226:                                              ; preds = %218
  %227 = getelementptr i8, ptr %222, i64 8
  store ptr %225, ptr %227
  br label %229
228:                                              ; preds = %218
  store ptr %225, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  br label %229
229:                                              ; preds = %228, %226
  %230 = getelementptr i8, ptr %143, i64 8
  %231 = load ptr, ptr %230
  %232 = icmp eq ptr %231, null
  br i1 %232, label %235, label %233
233:                                              ; preds = %229
  %234 = load ptr, ptr %143
  store ptr %234, ptr %231
  br label %235
235:                                              ; preds = %233, %229
  %236 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %237 = icmp eq ptr %236, %143
  br i1 %237, label %238, label %240
238:                                              ; preds = %235
  %239 = load ptr, ptr %230
  store ptr %239, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  br label %240
240:                                              ; preds = %238, %235
  %241 = load i64, ptr %144
  %242 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %243 = sub i64 %242, %241
  %244 = add i64 %243, -32
  store i64 %244, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %245 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %246 = add i64 %245, -1
  store i64 %246, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  tail call void @free(ptr %143)
  br label %251
247:                                              ; preds = %99, %110, %136
  %248 = tail call ptr @realloc(ptr %0, i64 %3)
  %249 = icmp eq ptr %248, null
  br i1 %249, label %250, label %251
250:                                              ; preds = %247
  call void @.gc.gc_resize.cold.2()
  unreachable
251:                                              ; preds = %247, %240, %214, %92, %44, %7
  %252 = phi ptr [ %8, %7 ], [ %69, %92 ], [ %0, %44 ], [ %203, %214 ], [ %221, %240 ], [ %248, %247 ]
  ret ptr %252
}
define internal fastcc ptr @.gc.pageOfObject(i64 %0) {
  %2 = icmp eq i64 %0, 0
  %3 = and i64 %0, 15
  %4 = icmp ne i64 %3, 0
  %5 = or i1 %2, %4
  %6 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %7 = icmp ult i64 %0, %6
  %8 = select i1 %5, i1 true, i1 %7
  %9 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %10 = icmp ugt i64 %0, %9
  %11 = select i1 %8, i1 true, i1 %10
  br i1 %11, label %48, label %12
12:                                               ; preds = %1
  %13 = and i64 %0, -65536
  %14 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1712)
  %15 = icmp eq i64 %14, 0
  br i1 %15, label %44, label %16
16:                                               ; preds = %12
  %17 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1704)
  %18 = lshr i64 %13, 4
  %19 = mul i64 %18, -7046029254386353131
  %20 = lshr i64 %19, 20
  %21 = add i64 %14, 17592186044415
  %22 = and i64 %21, %20
  %23 = getelementptr i64, ptr %17, i64 %22
  %24 = load i64, ptr %23
  %25 = icmp eq i64 %24, 0
  %26 = icmp eq i64 %24, %13
  %27 = or i1 %25, %26
  br i1 %27, label %39, label %28
28:                                               ; preds = %16
  %29 = add i64 %14, -1
  br label %30
30:                                               ; preds = %30, %28
  %31 = phi i64 [ %22, %28 ], [ %33, %30 ]
  %32 = add i64 %31, 1
  %33 = and i64 %32, %29
  %34 = getelementptr i64, ptr %17, i64 %33
  %35 = load i64, ptr %34
  %36 = icmp eq i64 %35, 0
  %37 = icmp eq i64 %35, %13
  %38 = or i1 %36, %37
  br i1 %38, label %39, label %30
39:                                               ; preds = %30, %16
  %40 = phi i64 [ %22, %16 ], [ %33, %30 ]
  %41 = getelementptr i64, ptr %17, i64 %40
  %42 = load i64, ptr %41
  %43 = icmp ne i64 %42, %13
  br label %44
44:                                               ; preds = %12, %39
  %45 = phi i1 [ true, %12 ], [ %43, %39 ]
  %46 = inttoptr i64 %13 to ptr
  %47 = select i1 %45, ptr null, ptr %46
  br label %48
48:                                               ; preds = %1, %44
  %49 = phi ptr [ %47, %44 ], [ null, %1 ]
  %50 = icmp eq ptr %49, null
  br i1 %50, label %83, label %51
51:                                               ; preds = %48
  %52 = ptrtoint ptr %49 to i64
  %53 = add i64 %52, 1072
  %54 = icmp ult i64 %0, %53
  br i1 %54, label %83, label %55
55:                                               ; preds = %51
  %56 = sub i64 %0, %53
  %57 = getelementptr i8, ptr %49, i64 8
  %58 = load i32, ptr %57
  %59 = zext i32 %58 to i64
  %60 = freeze i64 %56
  %61 = freeze i64 %59
  %62 = udiv i64 %60, %61
  %63 = mul i64 %62, %61
  %64 = sub i64 %60, %63
  %65 = icmp eq i64 %64, 0
  br i1 %65, label %66, label %83
66:                                               ; preds = %55
  %67 = getelementptr i8, ptr %49, i64 12
  %68 = load i32, ptr %67
  %69 = zext i32 %68 to i64
  %70 = icmp ult i64 %62, %69
  br i1 %70, label %71, label %83
71:                                               ; preds = %66
  %72 = getelementptr i8, ptr %49, i64 40
  %73 = lshr i64 %62, 6
  %74 = getelementptr i64, ptr %72, i64 %73
  %75 = load i64, ptr %74
  %76 = and i64 %62, 63
  %77 = shl i64 1, %76
  %78 = and i64 %75, %77
  %79 = icmp ne i64 %78, 0
  %80 = icmp sgt i64 %62, -1
  %81 = and i1 %80, %79
  %82 = select i1 %81, ptr %49, ptr null
  br label %83
83:                                               ; preds = %71, %66, %55, %51, %48
  %84 = phi ptr [ null, %48 ], [ null, %55 ], [ null, %51 ], [ null, %66 ], [ %82, %71 ]
  ret ptr %84
}
define linkonce_odr void @gc_release(ptr %0) {
  %2 = ptrtoint ptr %0 to i64
  %3 = load i32, ptr @gc_state
  %4 = icmp eq i32 %3, 0
  br i1 %4, label %7, label %5
5:                                                ; preds = %1
  %6 = tail call fastcc ptr @.gc.pageOfObject(i64 %2)
  br label %7
7:                                                ; preds = %1, %5
  %8 = phi ptr [ %6, %5 ], [ null, %1 ]
  %9 = icmp eq ptr %8, null
  br i1 %9, label %87, label %10
10:                                               ; preds = %7
  %11 = getelementptr i8, ptr %8, i64 8
  %12 = load i32, ptr %11
  %13 = load i32, ptr @gc_marking
  %14 = icmp eq i32 %13, 0
  br i1 %14, label %29, label %15
15:                                               ; preds = %10
  %16 = getelementptr i8, ptr %8, i64 16
  %17 = load i32, ptr %16
  store i32 1, ptr getelementptr (i8, ptr @gc_state, i64 1904)
  %18 = icmp eq i32 %17, 1
  br i1 %18, label %19, label %29
19:                                               ; preds = %15
  %20 = lshr i32 %12, 3
  %21 = zext i32 %20 to i64
  %22 = icmp ult i32 %12, 8
  br i1 %22, label %29, label %23
23:                                               ; preds = %19, %23
  %24 = phi i64 [ %27, %23 ], [ 0, %19 ]
  %25 = getelementptr i64, ptr %0, i64 %24
  %26 = load i64, ptr %25
  tail call fastcc void @.gc.markWord(i64 %26)
  %27 = add i64 %24, 1
  %28 = icmp eq i64 %27, %21
  br i1 %28, label %29, label %23
29:                                               ; preds = %23, %19, %15, %10
  %30 = ptrtoint ptr %8 to i64
  %31 = add i64 %30, 1072
  %32 = icmp ugt i64 %31, %2
  br i1 %32, label %58, label %33
33:                                               ; preds = %29
  %34 = sub i64 %2, %31
  %35 = load i32, ptr %11
  %36 = zext i32 %35 to i64
  %37 = freeze i64 %34
  %38 = freeze i64 %36
  %39 = udiv i64 %37, %38
  %40 = mul i64 %39, %38
  %41 = sub i64 %37, %40
  %42 = icmp eq i64 %41, 0
  br i1 %42, label %43, label %58
43:                                               ; preds = %33
  %44 = getelementptr i8, ptr %8, i64 12
  %45 = load i32, ptr %44
  %46 = zext i32 %45 to i64
  %47 = icmp ult i64 %39, %46
  br i1 %47, label %48, label %58
48:                                               ; preds = %43
  %49 = getelementptr i8, ptr %8, i64 40
  %50 = lshr i64 %39, 6
  %51 = getelementptr i64, ptr %49, i64 %50
  %52 = load i64, ptr %51
  %53 = and i64 %39, 63
  %54 = shl i64 1, %53
  %55 = and i64 %52, %54
  %56 = icmp eq i64 %55, 0
  %57 = select i1 %56, i64 -1, i64 %39
  br label %58
58:                                               ; preds = %48, %43, %33, %29
  %59 = phi i64 [ -1, %33 ], [ -1, %29 ], [ -1, %43 ], [ %57, %48 ]
  %60 = and i64 %59, 63
  %61 = shl i64 1, %60
  %62 = xor i64 %61, -1
  %63 = getelementptr i8, ptr %8, i64 40
  %64 = sdiv i64 %59, 64
  %65 = getelementptr [64 x i64], ptr %63, i64 0, i64 %64
  %66 = load i64, ptr %65
  %67 = and i64 %66, %62
  store i64 %67, ptr %65
  %68 = getelementptr i8, ptr %8, i64 552
  %69 = getelementptr [64 x i64], ptr %68, i64 0, i64 %64
  %70 = load i64, ptr %69
  %71 = and i64 %70, %62
  store i64 %71, ptr %69
  %72 = getelementptr i8, ptr %8, i64 20
  %73 = load i32, ptr %72
  %74 = add i32 %73, -1
  store i32 %74, ptr %72
  %75 = trunc i64 %64 to i32
  %76 = getelementptr i8, ptr %8, i64 28
  %77 = load i32, ptr %76
  %78 = icmp ugt i32 %77, %75
  br i1 %78, label %79, label %80
79:                                               ; preds = %58
  store i32 %75, ptr %76
  br label %80
80:                                               ; preds = %58, %79
  %81 = load i32, ptr %11
  %82 = zext i32 %81 to i64
  %83 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %84 = sub i64 %83, %82
  store i64 %84, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %85 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %86 = add i64 %85, -1
  store i64 %86, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  br label %177
87:                                               ; preds = %7
  br i1 %4, label %176, label %88
88:                                               ; preds = %87
  %89 = icmp eq ptr %0, null
  %90 = and i64 %2, 15
  %91 = icmp ne i64 %90, 0
  %92 = or i1 %89, %91
  %93 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %94 = icmp ugt i64 %93, %2
  %95 = select i1 %92, i1 true, i1 %94
  %96 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %97 = icmp ult i64 %96, %2
  %98 = select i1 %95, i1 true, i1 %97
  br i1 %98, label %176, label %99
99:                                               ; preds = %88
  %100 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1760)
  %101 = icmp eq i64 %100, 0
  br i1 %101, label %176, label %102
102:                                              ; preds = %99
  %103 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1752)
  %104 = lshr i64 %2, 4
  %105 = mul i64 %104, -7046029254386353131
  %106 = lshr i64 %105, 20
  %107 = add i64 %100, 17592186044415
  %108 = and i64 %107, %106
  %109 = getelementptr i64, ptr %103, i64 %108
  %110 = load i64, ptr %109
  %111 = icmp eq i64 %110, 0
  %112 = icmp eq i64 %110, %2
  %113 = or i1 %111, %112
  br i1 %113, label %125, label %114
114:                                              ; preds = %102
  %115 = add i64 %100, -1
  br label %116
116:                                              ; preds = %116, %114
  %117 = phi i64 [ %108, %114 ], [ %119, %116 ]
  %118 = add i64 %117, 1
  %119 = and i64 %118, %115
  %120 = getelementptr i64, ptr %103, i64 %119
  %121 = load i64, ptr %120
  %122 = icmp eq i64 %121, 0
  %123 = icmp eq i64 %121, %2
  %124 = or i1 %122, %123
  br i1 %124, label %125, label %116
125:                                              ; preds = %116, %102
  %126 = phi i64 [ %108, %102 ], [ %119, %116 ]
  %127 = getelementptr i64, ptr %103, i64 %126
  %128 = load i64, ptr %127
  %129 = icmp eq i64 %128, %2
  br i1 %129, label %130, label %176
130:                                              ; preds = %125
  %131 = add i64 %2, -32
  %132 = inttoptr i64 %131 to ptr
  %133 = getelementptr i8, ptr %132, i64 16
  %134 = load i64, ptr %133
  %135 = load i32, ptr @gc_marking
  %136 = icmp eq i32 %135, 0
  br i1 %136, label %150, label %137
137:                                              ; preds = %130
  %138 = getelementptr i8, ptr %132, i64 24
  %139 = load i32, ptr %138
  store i32 1, ptr getelementptr (i8, ptr @gc_state, i64 1904)
  %140 = icmp eq i32 %139, 1
  br i1 %140, label %141, label %150
141:                                              ; preds = %137
  %142 = lshr i64 %134, 3
  %143 = icmp ult i64 %134, 8
  br i1 %143, label %150, label %144
144:                                              ; preds = %141, %144
  %145 = phi i64 [ %148, %144 ], [ 0, %141 ]
  %146 = getelementptr i64, ptr %0, i64 %145
  %147 = load i64, ptr %146
  tail call fastcc void @.gc.markWord(i64 %147)
  %148 = add i64 %145, 1
  %149 = icmp eq i64 %148, %142
  br i1 %149, label %150, label %144
150:                                              ; preds = %144, %141, %137, %130
  tail call fastcc void @.gc.setRemove(ptr getelementptr (i8, ptr @gc_state, i64 1752), i64 %2)
  %151 = load ptr, ptr %132
  %152 = icmp eq ptr %151, null
  %153 = getelementptr i8, ptr %132, i64 8
  %154 = load ptr, ptr %153
  br i1 %152, label %157, label %155
155:                                              ; preds = %150
  %156 = getelementptr i8, ptr %151, i64 8
  store ptr %154, ptr %156
  br label %158
157:                                              ; preds = %150
  store ptr %154, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  br label %158
158:                                              ; preds = %157, %155
  %159 = getelementptr i8, ptr %132, i64 8
  %160 = load ptr, ptr %159
  %161 = icmp eq ptr %160, null
  br i1 %161, label %164, label %162
162:                                              ; preds = %158
  %163 = load ptr, ptr %132
  store ptr %163, ptr %160
  br label %164
164:                                              ; preds = %162, %158
  %165 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %166 = icmp eq ptr %165, %132
  br i1 %166, label %167, label %169
167:                                              ; preds = %164
  %168 = load ptr, ptr %159
  store ptr %168, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  br label %169
169:                                              ; preds = %164, %167
  %170 = load i64, ptr %133
  %171 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %172 = sub i64 %171, %170
  %173 = add i64 %172, -32
  store i64 %173, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %174 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %175 = add i64 %174, -1
  store i64 %175, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  tail call void @free(ptr %132)
  br label %177
176:                                              ; preds = %88, %99, %125, %87
  tail call void @free(ptr %0)
  br label %177
177:                                              ; preds = %169, %176, %80
  ret void
}
define linkonce_odr void @gc_free_hint(ptr %0) {
  %2 = load i32, ptr @gc_state
  %3 = icmp eq i32 %2, 0
  br i1 %3, label %4, label %5
4:                                                ; preds = %1
  tail call void @free(ptr %0)
  br label %5
5:                                                ; preds = %4, %1
  ret void
}
define linkonce_odr void @gc_push_frame(ptr %0, i64 %1) {
  %3 = getelementptr i8, ptr %0, i64 8
  store i64 %1, ptr %3
  %4 = getelementptr i8, ptr %0, i64 16
  %5 = shl i64 %1, 3
  tail call void @llvm.memset.p0.i64(ptr %4, i8 0, i64 %5, i1 false)
  %6 = load ptr, ptr @gc_top
  store ptr %6, ptr %0
  store ptr %0, ptr @gc_top
  ret void
}
define linkonce_odr void @gc_pop_frame(ptr %0) {
  %2 = load ptr, ptr %0
  store ptr %2, ptr @gc_top
  ret void
}
define linkonce_odr void @gc_link_frame(ptr %0, ptr %1) {
  %3 = load i32, ptr @gc_marking
  %4 = icmp eq i32 %3, 0
  br i1 %4, label %140, label %5
5:                                                ; preds = %2
  %6 = ptrtoint ptr %1 to i64
  %7 = load i32, ptr @gc_state
  %8 = icmp eq i32 %7, 0
  br i1 %8, label %140, label %9
9:                                                ; preds = %5
  %10 = icmp eq ptr %1, null
  %11 = and i64 %6, 15
  %12 = icmp ne i64 %11, 0
  %13 = or i1 %10, %12
  %14 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %15 = icmp ugt i64 %14, %6
  %16 = select i1 %13, i1 true, i1 %15
  %17 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %18 = icmp ult i64 %17, %6
  %19 = select i1 %16, i1 true, i1 %18
  br i1 %19, label %140, label %20
20:                                               ; preds = %9
  %21 = and i64 %6, -65536
  %22 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1712)
  %23 = icmp eq i64 %22, 0
  br i1 %23, label %52, label %24
24:                                               ; preds = %20
  %25 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1704)
  %26 = lshr i64 %21, 4
  %27 = mul i64 %26, -7046029254386353131
  %28 = lshr i64 %27, 20
  %29 = add i64 %22, 17592186044415
  %30 = and i64 %29, %28
  %31 = getelementptr i64, ptr %25, i64 %30
  %32 = load i64, ptr %31
  %33 = icmp eq i64 %32, 0
  %34 = icmp eq i64 %32, %21
  %35 = or i1 %33, %34
  br i1 %35, label %47, label %36
36:                                               ; preds = %24
  %37 = add i64 %22, -1
  br label %38
38:                                               ; preds = %38, %36
  %39 = phi i64 [ %30, %36 ], [ %41, %38 ]
  %40 = add i64 %39, 1
  %41 = and i64 %40, %37
  %42 = getelementptr i64, ptr %25, i64 %41
  %43 = load i64, ptr %42
  %44 = icmp eq i64 %43, 0
  %45 = icmp eq i64 %43, %21
  %46 = or i1 %44, %45
  br i1 %46, label %47, label %38
47:                                               ; preds = %38, %24
  %48 = phi i64 [ %30, %24 ], [ %41, %38 ]
  %49 = getelementptr i64, ptr %25, i64 %48
  %50 = load i64, ptr %49
  %51 = icmp ne i64 %50, %21
  br label %52
52:                                               ; preds = %47, %20
  %53 = phi i1 [ true, %20 ], [ %51, %47 ]
  %54 = inttoptr i64 %21 to ptr
  %55 = icmp eq i64 %21, 0
  %56 = or i1 %55, %53
  br i1 %56, label %96, label %57
57:                                               ; preds = %52
  %58 = or i64 %21, 1072
  %59 = icmp ugt i64 %58, %6
  br i1 %59, label %140, label %60
60:                                               ; preds = %57
  %61 = sub i64 %6, %58
  %62 = getelementptr i8, ptr %54, i64 8
  %63 = load i32, ptr %62
  %64 = zext i32 %63 to i64
  %65 = freeze i64 %64
  %66 = udiv i64 %61, %65
  %67 = mul i64 %66, %65
  %68 = sub i64 %61, %67
  %69 = icmp eq i64 %68, 0
  br i1 %69, label %70, label %140
70:                                               ; preds = %60
  %71 = getelementptr i8, ptr %54, i64 12
  %72 = load i32, ptr %71
  %73 = zext i32 %72 to i64
  %74 = icmp ult i64 %66, %73
  br i1 %74, label %75, label %140
75:                                               ; preds = %70
  %76 = getelementptr i8, ptr %54, i64 40
  %77 = lshr i64 %66, 6
  %78 = getelementptr i64, ptr %76, i64 %77
  %79 = load i64, ptr %78
  %80 = and i64 %66, 63
  %81 = shl i64 1, %80
  %82 = and i64 %79, %81
  %83 = icmp ne i64 %82, 0
  %84 = icmp sgt i64 %66, -1
  %85 = and i1 %84, %83
  br i1 %85, label %86, label %140
86:                                               ; preds = %75
  %87 = lshr i32 %63, 3
  %88 = zext i32 %87 to i64
  %89 = icmp ult i32 %63, 8
  br i1 %89, label %140, label %90
90:                                               ; preds = %86, %90
  %91 = phi i64 [ %94, %90 ], [ 0, %86 ]
  %92 = getelementptr i64, ptr %1, i64 %91
  %93 = load i64, ptr %92
  tail call fastcc void @.gc.markWord(i64 %93)
  %94 = add i64 %91, 1
  %95 = icmp eq i64 %94, %88
  br i1 %95, label %140, label %90
96:                                               ; preds = %52
  %97 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1760)
  %98 = icmp eq i64 %97, 0
  br i1 %98, label %140, label %99
99:                                               ; preds = %96
  %100 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1752)
  %101 = lshr i64 %6, 4
  %102 = mul i64 %101, -7046029254386353131
  %103 = lshr i64 %102, 20
  %104 = add i64 %97, 17592186044415
  %105 = and i64 %104, %103
  %106 = getelementptr i64, ptr %100, i64 %105
  %107 = load i64, ptr %106
  %108 = icmp eq i64 %107, 0
  %109 = icmp eq i64 %107, %6
  %110 = or i1 %108, %109
  br i1 %110, label %122, label %111
111:                                              ; preds = %99
  %112 = add i64 %97, -1
  br label %113
113:                                              ; preds = %113, %111
  %114 = phi i64 [ %105, %111 ], [ %116, %113 ]
  %115 = add i64 %114, 1
  %116 = and i64 %115, %112
  %117 = getelementptr i64, ptr %100, i64 %116
  %118 = load i64, ptr %117
  %119 = icmp eq i64 %118, 0
  %120 = icmp eq i64 %118, %6
  %121 = or i1 %119, %120
  br i1 %121, label %122, label %113
122:                                              ; preds = %113, %99
  %123 = phi i64 [ %105, %99 ], [ %116, %113 ]
  %124 = getelementptr i64, ptr %100, i64 %123
  %125 = load i64, ptr %124
  %126 = icmp eq i64 %125, %6
  br i1 %126, label %127, label %140
127:                                              ; preds = %122
  %128 = add i64 %6, -32
  %129 = inttoptr i64 %128 to ptr
  %130 = getelementptr i8, ptr %129, i64 16
  %131 = load i64, ptr %130
  %132 = lshr i64 %131, 3
  %133 = icmp ult i64 %131, 8
  br i1 %133, label %140, label %134
134:                                              ; preds = %127, %134
  %135 = phi i64 [ %138, %134 ], [ 0, %127 ]
  %136 = getelementptr i64, ptr %1, i64 %135
  %137 = load i64, ptr %136
  tail call fastcc void @.gc.markWord(i64 %137)
  %138 = add i64 %135, 1
  %139 = icmp eq i64 %138, %132
  br i1 %139, label %140, label %134
140:                                              ; preds = %90, %134, %127, %122, %96, %86, %9, %5, %75, %70, %60, %57, %2
  %141 = load ptr, ptr @gc_top
  store ptr %141, ptr %0
  store ptr %0, ptr @gc_top
  ret void
}
define linkonce_odr void @gc_unlink_frame(ptr %0) {
  %2 = load ptr, ptr %0
  store ptr %2, ptr @gc_top
  ret void
}
define linkonce_odr void @gc_start(ptr %0, i64 %1) {
  store ptr %0, ptr getelementptr (i8, ptr @gc_state, i64 1840)
  store i64 %1, ptr getelementptr (i8, ptr @gc_state, i64 1848)
  %3 = tail call ptr @getenv(ptr @.gc_str.3)
  %4 = icmp eq ptr %3, null
  br i1 %4, label %9, label %5
5:                                                ; preds = %2
  %6 = tail call i32 @strcmp(ptr %3, ptr @.gc_str.4)
  %7 = icmp eq i32 %6, 0
  %8 = zext i1 %7 to i32
  br label %9
9:                                                ; preds = %5, %2
  %10 = phi i32 [ 0, %2 ], [ %8, %5 ]
  store i32 %10, ptr getelementptr (i8, ptr @gc_state, i64 16)
  %11 = icmp eq i32 %10, 0
  br i1 %11, label %12, label %22
12:                                               ; preds = %9
  %13 = tail call ptr @getenv(ptr @.gc_str.3)
  %14 = icmp eq ptr %13, null
  br i1 %14, label %22, label %15
15:                                               ; preds = %12
  %16 = load i8, ptr %13
  %17 = icmp eq i8 %16, 0
  br i1 %17, label %22, label %18
18:                                               ; preds = %15
  %19 = tail call i64 @strtoull(ptr %13, ptr null, i32 10)
  %20 = icmp ne i64 %19, 0
  %21 = zext i1 %20 to i32
  br label %22
22:                                               ; preds = %18, %15, %12, %9
  %23 = phi i32 [ 0, %9 ], [ %21, %18 ], [ 0, %15 ], [ 0, %12 ]
  store i32 %23, ptr getelementptr (i8, ptr @gc_state, i64 12)
  %24 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 16)
  %25 = icmp eq i32 %24, 0
  br i1 %25, label %26, label %42
26:                                               ; preds = %22
  %27 = tail call ptr @getenv(ptr @.gc_str.5)
  %28 = icmp eq ptr %27, null
  br i1 %28, label %33, label %29
29:                                               ; preds = %26
  %30 = tail call i32 @strcmp(ptr %27, ptr @.gc_str.6)
  %31 = icmp ne i32 %30, 0
  %32 = zext i1 %31 to i32
  br label %33
33:                                               ; preds = %29, %26
  %34 = phi i32 [ 1, %26 ], [ %32, %29 ]
  store i32 %34, ptr getelementptr (i8, ptr @gc_state, i64 8)
  %35 = tail call ptr @getenv(ptr @.gc_str.9)
  %36 = icmp eq ptr %35, null
  br i1 %36, label %43, label %37
37:                                               ; preds = %33
  %38 = load i8, ptr %35
  %39 = icmp eq i8 %38, 0
  br i1 %39, label %43, label %40
40:                                               ; preds = %37
  %41 = tail call i64 @strtoull(ptr %35, ptr null, i32 10)
  br label %43
42:                                               ; preds = %22
  store i32 1, ptr getelementptr (i8, ptr @gc_state, i64 8)
  br label %43
43:                                               ; preds = %42, %40, %37, %33
  %44 = phi i64 [ %41, %40 ], [ 10000, %37 ], [ 10000, %33 ], [ 1, %42 ]
  %45 = tail call i64 @llvm.umax.i64(i64 %44, i64 1)
  store i64 %45, ptr getelementptr (i8, ptr @gc_state, i64 1880)
  %46 = tail call ptr @getenv(ptr @.gc_str.7)
  %47 = icmp eq ptr %46, null
  br i1 %47, label %55, label %48
48:                                               ; preds = %43
  %49 = load i8, ptr %46
  %50 = icmp eq i8 %49, 0
  br i1 %50, label %55, label %51
51:                                               ; preds = %48
  %52 = tail call i64 @strtoull(ptr %46, ptr null, i32 10)
  %53 = icmp ne i64 %52, 0
  %54 = zext i1 %53 to i32
  br label %55
55:                                               ; preds = %51, %48, %43
  %56 = phi i32 [ %54, %51 ], [ 0, %48 ], [ 0, %43 ]
  store i32 %56, ptr getelementptr (i8, ptr @gc_state, i64 20)
  %57 = tail call ptr @getenv(ptr @.gc_str.8)
  %58 = icmp eq ptr %57, null
  br i1 %58, label %64, label %59
59:                                               ; preds = %55
  %60 = load i8, ptr %57
  %61 = icmp eq i8 %60, 0
  br i1 %61, label %64, label %62
62:                                               ; preds = %59
  %63 = tail call i64 @strtoull(ptr %57, ptr null, i32 10)
  br label %64
64:                                               ; preds = %55, %59, %62
  %65 = phi i64 [ %63, %62 ], [ 4294967296, %59 ], [ 4294967296, %55 ]
  store i64 %65, ptr getelementptr (i8, ptr @gc_state, i64 1832)
  br label %84
66:                                               ; preds = %96, %79
  %67 = phi i64 [ %82, %79 ], [ 0, %96 ]
  %68 = phi i64 [ %74, %79 ], [ 0, %96 ]
  %69 = shl i64 %68, 32
  %70 = ashr i64 %69, 32
  %71 = trunc i64 %67 to i32
  %72 = shl i32 %71, 4
  br label %73
73:                                               ; preds = %73, %66
  %74 = phi i64 [ %78, %73 ], [ %70, %66 ]
  %75 = getelementptr [24 x i32], ptr @.gc.kClassBytes, i64 0, i64 %74
  %76 = load i32, ptr %75
  %77 = icmp ugt i32 %72, %76
  %78 = add i64 %74, 1
  br i1 %77, label %73, label %79
79:                                               ; preds = %73
  %80 = trunc i64 %74 to i8
  %81 = getelementptr [129 x i8], ptr getelementptr (i8, ptr @gc_state, i64 1568), i64 0, i64 %67
  store i8 %80, ptr %81
  %82 = add i64 %67, 1
  %83 = icmp eq i64 %82, 129
  br i1 %83, label %99, label %66
84:                                               ; preds = %96, %64
  %85 = phi i64 [ 0, %64 ], [ %97, %96 ]
  %86 = trunc i64 %85 to i32
  br label %87
87:                                               ; preds = %87, %84
  %88 = phi i64 [ 0, %84 ], [ %94, %87 ]
  %89 = getelementptr [24 x i32], ptr @.gc.kClassBytes, i64 0, i64 %88
  %90 = load i32, ptr %89
  %91 = getelementptr [2 x [24 x %struct.GcClass]], ptr getelementptr (i8, ptr @gc_state, i64 32), i64 0, i64 %85, i64 %88
  %92 = getelementptr i8, ptr %91, i64 24
  store i32 %90, ptr %92
  %93 = getelementptr i8, ptr %91, i64 28
  store i32 %86, ptr %93
  %94 = add i64 %88, 1
  %95 = icmp eq i64 %94, 24
  br i1 %95, label %96, label %87
96:                                               ; preds = %87
  %97 = add i64 %85, 1
  %98 = icmp eq i64 %85, 0
  br i1 %98, label %84, label %66
99:                                               ; preds = %79
  %100 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1832)
  %101 = tail call i64 @llvm.umin.i64(i64 %100, i64 8388608)
  %102 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 12)
  %103 = icmp eq i32 %102, 0
  %104 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 16)
  %105 = icmp eq i32 %104, 0
  %106 = select i1 %103, i1 %105, i1 false
  %107 = select i1 %106, i64 %101, i64 0
  store i64 %107, ptr getelementptr (i8, ptr @gc_state, i64 1824)
  store i64 %107, ptr getelementptr (i8, ptr @gc_state, i64 1816)
  store i32 1, ptr @gc_state
  %108 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 20)
  %109 = icmp eq i32 %108, 0
  br i1 %109, label %112, label %110
110:                                              ; preds = %99
  %111 = tail call i32 @atexit(ptr @.gc.printTheReport)
  br label %112
112:                                              ; preds = %110, %99
  ret void
}
define internal void @.gc.printTheReport() {
  %1 = alloca [32 x i8]
  %2 = alloca [32 x i8]
  %3 = alloca [32 x i8]
  %4 = alloca [32 x i8]
  %5 = alloca [300 x i8]
  %6 = alloca [200 x i8]
  %7 = alloca [460 x i8]
  %8 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %9 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9952)
  %10 = icmp ugt i64 %8, %9
  br i1 %10, label %11, label %12
11:                                               ; preds = %0
  store i64 %8, ptr getelementptr (i8, ptr @gc_state, i64 9952)
  br label %12
12:                                               ; preds = %0, %11
  %13 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9928)
  %14 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9936)
  %15 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9944)
  %16 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9952)
  %17 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %18 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1720)
  %19 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1768)
  %20 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %5, i64 300, ptr @.gc_str.12, i64 %13, i64 %14, i64 %15, i64 %16, i64 %8, i64 %17, i64 %18, i64 %19)
  %21 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  %22 = freeze i64 %21
  %23 = udiv i64 %22, 1000
  %24 = mul i64 %23, 1000
  %25 = sub i64 %22, %24
  %26 = udiv i64 %25, 100
  %27 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %1, i64 32, ptr @.gc_str.14, i64 %23, i64 %26)
  %28 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1936)
  %29 = mul i64 %28, 50
  %30 = add i64 %29, 99
  %31 = udiv i64 %30, 100
  br label %35
32:                                               ; preds = %35
  %33 = add i64 %36, 1
  %34 = icmp eq i64 %33, 1992
  br i1 %34, label %45, label %35
35:                                               ; preds = %32, %12
  %36 = phi i64 [ 0, %12 ], [ %33, %32 ]
  %37 = phi i64 [ 0, %12 ], [ %41, %32 ]
  %38 = getelementptr [1992 x i32], ptr getelementptr (i8, ptr @gc_state, i64 1960), i64 0, i64 %36
  %39 = load i32, ptr %38
  %40 = zext i32 %39 to i64
  %41 = add i64 %37, %40
  %42 = icmp uge i64 %41, %31
  %43 = icmp ne i64 %41, 0
  %44 = and i1 %42, %43
  br i1 %44, label %45, label %32
45:                                               ; preds = %35, %32
  %46 = phi i64 [ -1, %32 ], [ %36, %35 ]
  %47 = icmp slt i64 %46, 0
  br i1 %47, label %48, label %50
48:                                               ; preds = %45
  %49 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  br label %62
50:                                               ; preds = %45
  %51 = trunc i64 %46 to i32
  %52 = icmp ult i32 %51, 1000
  %53 = mul i64 %46, 1000
  %54 = add i64 %53, 1000
  %55 = and i64 %54, 4294967288
  %56 = and i64 %46, 4294967295
  %57 = mul i64 %56, 100000
  %58 = add i64 %57, -98900000
  %59 = select i1 %52, i64 %55, i64 %58
  %60 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  %61 = tail call i64 @llvm.umin.i64(i64 %59, i64 %60)
  br label %62
62:                                               ; preds = %50, %48
  %63 = phi i64 [ %49, %48 ], [ %61, %50 ]
  %64 = freeze i64 %63
  %65 = udiv i64 %64, 1000
  %66 = mul i64 %65, 1000
  %67 = sub i64 %64, %66
  %68 = udiv i64 %67, 100
  %69 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %2, i64 32, ptr @.gc_str.14, i64 %65, i64 %68)
  %70 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1936)
  %71 = mul i64 %70, 99
  %72 = add i64 %71, 99
  %73 = udiv i64 %72, 100
  br label %77
74:                                               ; preds = %77
  %75 = add i64 %78, 1
  %76 = icmp eq i64 %75, 1992
  br i1 %76, label %87, label %77
77:                                               ; preds = %74, %62
  %78 = phi i64 [ 0, %62 ], [ %75, %74 ]
  %79 = phi i64 [ 0, %62 ], [ %83, %74 ]
  %80 = getelementptr [1992 x i32], ptr getelementptr (i8, ptr @gc_state, i64 1960), i64 0, i64 %78
  %81 = load i32, ptr %80
  %82 = zext i32 %81 to i64
  %83 = add i64 %79, %82
  %84 = icmp uge i64 %83, %73
  %85 = icmp ne i64 %83, 0
  %86 = and i1 %84, %85
  br i1 %86, label %87, label %74
87:                                               ; preds = %77, %74
  %88 = phi i64 [ -1, %74 ], [ %78, %77 ]
  %89 = icmp slt i64 %88, 0
  br i1 %89, label %90, label %92
90:                                               ; preds = %87
  %91 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  br label %104
92:                                               ; preds = %87
  %93 = trunc i64 %88 to i32
  %94 = icmp ult i32 %93, 1000
  %95 = mul i64 %88, 1000
  %96 = add i64 %95, 1000
  %97 = and i64 %96, 4294967288
  %98 = and i64 %88, 4294967295
  %99 = mul i64 %98, 100000
  %100 = add i64 %99, -98900000
  %101 = select i1 %94, i64 %97, i64 %100
  %102 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1944)
  %103 = tail call i64 @llvm.umin.i64(i64 %101, i64 %102)
  br label %104
104:                                              ; preds = %90, %92
  %105 = phi i64 [ %91, %90 ], [ %103, %92 ]
  %106 = freeze i64 %105
  %107 = udiv i64 %106, 1000
  %108 = mul i64 %107, 1000
  %109 = sub i64 %106, %108
  %110 = udiv i64 %109, 100
  %111 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %3, i64 32, ptr @.gc_str.14, i64 %107, i64 %110)
  %112 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1952)
  %113 = freeze i64 %112
  %114 = udiv i64 %113, 1000
  %115 = mul i64 %114, 1000
  %116 = sub i64 %113, %115
  %117 = udiv i64 %116, 100
  %118 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %4, i64 32, ptr @.gc_str.14, i64 %114, i64 %117)
  %119 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1936)
  %120 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %6, i64 200, ptr @.gc_str.13, i64 %119, ptr %1, ptr %2, ptr %3, ptr %4)
  %121 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 8)
  %122 = icmp ne i32 %121, 0
  %123 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 12)
  %124 = icmp eq i32 %123, 0
  %125 = select i1 %122, i1 %124, i1 false
  %126 = select i1 %125, ptr @.gc_str.4, ptr @.gc_str.11
  %127 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %7, i64 460, ptr @.gc_str.10, ptr %5, ptr %6, ptr %126)
  %128 = call i64 @strlen(ptr %7)
  %129 = call i64 @write(i32 2, ptr %7, i64 %128)
  ret void
}
define internal fastcc void @.gc.markASlice(i64 %0) {
  %2 = alloca %struct.GcSet
  %3 = icmp eq i64 %0, 0
  %4 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %5 = icmp eq i64 %4, 0
  %6 = select i1 %3, i1 true, i1 %5
  br i1 %6, label %149, label %7
7:                                                ; preds = %1, %142
  %8 = phi i64 [ %146, %142 ], [ %4, %1 ]
  %9 = phi i64 [ %144, %142 ], [ 0, %1 ]
  %10 = sub i64 %0, %9
  %11 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  %12 = add i64 %8, -1
  store i64 %12, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %13 = getelementptr %struct.GcGrey, ptr %11, i64 %12
  %14 = load i64, ptr %13
  %15 = getelementptr i8, ptr %13, i64 8
  %16 = load i64, ptr %15
  %17 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 1904)
  %18 = icmp eq i32 %17, 0
  br i1 %18, label %88, label %19
19:                                               ; preds = %7
  %20 = and i64 %14, -2
  %21 = and i64 %14, 1
  %22 = icmp eq i64 %21, 0
  br i1 %22, label %23, label %54
23:                                               ; preds = %19
  %24 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1760)
  %25 = icmp eq i64 %24, 0
  br i1 %25, label %142, label %26
26:                                               ; preds = %23
  %27 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1752)
  %28 = lshr i64 %14, 4
  %29 = mul i64 %28, -7046029254386353131
  %30 = lshr i64 %29, 20
  %31 = add i64 %24, 17592186044415
  %32 = and i64 %31, %30
  %33 = getelementptr i64, ptr %27, i64 %32
  %34 = load i64, ptr %33
  %35 = icmp eq i64 %34, 0
  %36 = icmp eq i64 %34, %20
  %37 = or i1 %35, %36
  br i1 %37, label %49, label %38
38:                                               ; preds = %26
  %39 = add i64 %24, -1
  br label %40
40:                                               ; preds = %40, %38
  %41 = phi i64 [ %32, %38 ], [ %43, %40 ]
  %42 = add i64 %41, 1
  %43 = and i64 %42, %39
  %44 = getelementptr i64, ptr %27, i64 %43
  %45 = load i64, ptr %44
  %46 = icmp eq i64 %45, 0
  %47 = icmp eq i64 %45, %20
  %48 = or i1 %46, %47
  br i1 %48, label %49, label %40
49:                                               ; preds = %40, %26
  %50 = phi i64 [ %32, %26 ], [ %43, %40 ]
  %51 = getelementptr i64, ptr %27, i64 %50
  %52 = load i64, ptr %51
  %53 = icmp eq i64 %52, %20
  br label %86
54:                                               ; preds = %19
  %55 = and i64 %14, -65536
  %56 = inttoptr i64 %55 to ptr
  %57 = or i64 %55, 1072
  %58 = icmp ult i64 %20, %57
  br i1 %58, label %142, label %59
59:                                               ; preds = %54
  %60 = sub i64 %20, %57
  %61 = getelementptr i8, ptr %56, i64 8
  %62 = load i32, ptr %61
  %63 = zext i32 %62 to i64
  %64 = freeze i64 %60
  %65 = freeze i64 %63
  %66 = udiv i64 %64, %65
  %67 = mul i64 %66, %65
  %68 = sub i64 %64, %67
  %69 = icmp eq i64 %68, 0
  br i1 %69, label %70, label %142
70:                                               ; preds = %59
  %71 = getelementptr i8, ptr %56, i64 12
  %72 = load i32, ptr %71
  %73 = zext i32 %72 to i64
  %74 = icmp ult i64 %66, %73
  br i1 %74, label %75, label %142
75:                                               ; preds = %70
  %76 = getelementptr i8, ptr %56, i64 40
  %77 = lshr i64 %66, 6
  %78 = getelementptr i64, ptr %76, i64 %77
  %79 = load i64, ptr %78
  %80 = and i64 %66, 63
  %81 = shl i64 1, %80
  %82 = and i64 %79, %81
  %83 = icmp ne i64 %82, 0
  %84 = icmp sgt i64 %66, -1
  %85 = and i1 %84, %83
  br label %86
86:                                               ; preds = %75, %49
  %87 = phi i1 [ %53, %49 ], [ %85, %75 ]
  br i1 %87, label %88, label %142
88:                                               ; preds = %86, %7
  %89 = and i64 %14, 1
  %90 = icmp eq i64 %89, 0
  br i1 %90, label %98, label %91
91:                                               ; preds = %88
  %92 = and i64 %14, -65536
  %93 = inttoptr i64 %92 to ptr
  %94 = getelementptr i8, ptr %93, i64 8
  %95 = load i32, ptr %94
  %96 = lshr i32 %95, 3
  %97 = zext i32 %96 to i64
  br label %104
98:                                               ; preds = %88
  %99 = add i64 %14, -32
  %100 = inttoptr i64 %99 to ptr
  %101 = getelementptr i8, ptr %100, i64 16
  %102 = load i64, ptr %101
  %103 = lshr i64 %102, 3
  br label %104
104:                                              ; preds = %98, %91
  %105 = phi i64 [ %97, %91 ], [ %103, %98 ]
  %106 = tail call i64 @llvm.umin.i64(i64 %16, i64 %105)
  %107 = sub i64 %105, %106
  %108 = tail call i64 @llvm.umin.i64(i64 %10, i64 %107)
  %109 = tail call i64 @llvm.umin.i64(i64 %108, i64 4096)
  %110 = add i64 %109, %106
  %111 = icmp ult i64 %110, %105
  br i1 %111, label %112, label %130
112:                                              ; preds = %104
  %113 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1872)
  %114 = icmp ult i64 %12, %113
  br i1 %114, label %124, label %115
115:                                              ; preds = %112
  %116 = icmp eq i64 %113, 0
  %117 = shl i64 %113, 1
  %118 = select i1 %116, i64 256, i64 %117
  store i64 %118, ptr getelementptr (i8, ptr @gc_state, i64 1872)
  %119 = shl i64 %118, 4
  %120 = tail call ptr @realloc(ptr %11, i64 %119)
  %121 = icmp eq ptr %120, null
  br i1 %121, label %122, label %123
122:                                              ; preds = %115
  call void @.gc.markASlice.cold.1()
  unreachable
123:                                              ; preds = %115
  store ptr %120, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  br label %124
124:                                              ; preds = %123, %112
  %125 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1856)
  %126 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %127 = add i64 %126, 1
  store i64 %127, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %128 = getelementptr %struct.GcGrey, ptr %125, i64 %126
  store i64 %14, ptr %128
  %129 = getelementptr i8, ptr %128, i64 8
  store i64 %110, ptr %129
  br label %130
130:                                              ; preds = %124, %104
  %131 = and i64 %14, -2
  %132 = inttoptr i64 %131 to ptr
  %133 = icmp eq i64 %108, 0
  br i1 %133, label %140, label %134
134:                                              ; preds = %130, %134
  %135 = phi i64 [ %138, %134 ], [ %106, %130 ]
  %136 = getelementptr i64, ptr %132, i64 %135
  %137 = load i64, ptr %136
  tail call fastcc void @.gc.markWord(i64 %137)
  %138 = add i64 %135, 1
  %139 = icmp eq i64 %138, %110
  br i1 %139, label %140, label %134
140:                                              ; preds = %134, %130
  %141 = add i64 %109, 1
  br label %142
142:                                              ; preds = %23, %54, %59, %70, %86, %140
  %143 = phi i64 [ %141, %140 ], [ 1, %86 ], [ 1, %70 ], [ 1, %54 ], [ 1, %59 ], [ 1, %23 ]
  %144 = add i64 %143, %9
  %145 = icmp uge i64 %144, %0
  %146 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1864)
  %147 = icmp eq i64 %146, 0
  %148 = select i1 %145, i1 true, i1 %147
  br i1 %148, label %149, label %7
149:                                              ; preds = %142, %1
  %150 = phi i1 [ %5, %1 ], [ %147, %142 ]
  br i1 %150, label %151, label %496
151:                                              ; preds = %149
  %152 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 16)
  %153 = icmp eq i32 %152, 0
  br i1 %153, label %492, label %154
154:                                              ; preds = %151
  %155 = load ptr, ptr @gc_top
  %156 = icmp eq ptr %155, null
  br i1 %156, label %173, label %163
157:                                              ; preds = %196, %163
  %158 = phi ptr [ %164, %163 ], [ %197, %196 ]
  %159 = phi i64 [ %165, %163 ], [ %199, %196 ]
  %160 = phi i64 [ %166, %163 ], [ %198, %196 ]
  %161 = load ptr, ptr %167
  %162 = icmp eq ptr %161, null
  br i1 %162, label %173, label %163
163:                                              ; preds = %154, %157
  %164 = phi ptr [ %158, %157 ], [ null, %154 ]
  %165 = phi i64 [ %159, %157 ], [ 0, %154 ]
  %166 = phi i64 [ %160, %157 ], [ 0, %154 ]
  %167 = phi ptr [ %161, %157 ], [ %155, %154 ]
  %168 = getelementptr i8, ptr %167, i64 8
  %169 = load i64, ptr %168
  %170 = icmp sgt i64 %169, 0
  br i1 %170, label %171, label %157
171:                                              ; preds = %163
  %172 = getelementptr i8, ptr %167, i64 16
  br label %179
173:                                              ; preds = %157, %154
  %174 = phi ptr [ null, %154 ], [ %158, %157 ]
  %175 = phi i64 [ 0, %154 ], [ %159, %157 ]
  %176 = phi i64 [ 0, %154 ], [ %160, %157 ]
  %177 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1848)
  %178 = icmp sgt i64 %177, 0
  br i1 %178, label %205, label %233
179:                                              ; preds = %196, %171
  %180 = phi ptr [ %164, %171 ], [ %197, %196 ]
  %181 = phi i64 [ %165, %171 ], [ %199, %196 ]
  %182 = phi i64 [ %166, %171 ], [ %198, %196 ]
  %183 = phi i64 [ 0, %171 ], [ %202, %196 ]
  %184 = getelementptr [0 x ptr], ptr %172, i64 0, i64 %183
  %185 = load ptr, ptr %184
  %186 = ptrtoint ptr %185 to i64
  %187 = icmp ult i64 %181, %182
  br i1 %187, label %196, label %188
188:                                              ; preds = %179
  %189 = icmp eq i64 %182, 0
  %190 = shl i64 %182, 1
  %191 = select i1 %189, i64 256, i64 %190
  %192 = shl i64 %191, 4
  %193 = tail call ptr @realloc(ptr %180, i64 %192)
  %194 = icmp eq ptr %193, null
  br i1 %194, label %195, label %196
195:                                              ; preds = %188
  call void @.gc.markASlice.cold.2()
  unreachable
196:                                              ; preds = %188, %179
  %197 = phi ptr [ %180, %179 ], [ %193, %188 ]
  %198 = phi i64 [ %182, %179 ], [ %191, %188 ]
  %199 = add i64 %181, 1
  %200 = getelementptr %struct.GcGrey, ptr %197, i64 %181
  store i64 %186, ptr %200
  %201 = getelementptr i8, ptr %200, i64 8
  store i64 0, ptr %201
  %202 = add i64 %183, 1
  %203 = load i64, ptr %168
  %204 = icmp slt i64 %202, %203
  br i1 %204, label %179, label %157
205:                                              ; preds = %173, %224
  %206 = phi ptr [ %225, %224 ], [ %174, %173 ]
  %207 = phi i64 [ %227, %224 ], [ %175, %173 ]
  %208 = phi i64 [ %226, %224 ], [ %176, %173 ]
  %209 = phi i64 [ %230, %224 ], [ 0, %173 ]
  %210 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1840)
  %211 = getelementptr ptr, ptr %210, i64 %209
  %212 = load ptr, ptr %211
  %213 = load ptr, ptr %212
  %214 = ptrtoint ptr %213 to i64
  %215 = icmp ult i64 %207, %208
  br i1 %215, label %224, label %216
216:                                              ; preds = %205
  %217 = icmp eq i64 %208, 0
  %218 = shl i64 %208, 1
  %219 = select i1 %217, i64 256, i64 %218
  %220 = shl i64 %219, 4
  %221 = tail call ptr @realloc(ptr %206, i64 %220)
  %222 = icmp eq ptr %221, null
  br i1 %222, label %223, label %224
223:                                              ; preds = %216
  call void @.gc.markASlice.cold.3()
  unreachable
224:                                              ; preds = %216, %205
  %225 = phi ptr [ %206, %205 ], [ %221, %216 ]
  %226 = phi i64 [ %208, %205 ], [ %219, %216 ]
  %227 = add i64 %207, 1
  %228 = getelementptr %struct.GcGrey, ptr %225, i64 %207
  store i64 %214, ptr %228
  %229 = getelementptr i8, ptr %228, i64 8
  store i64 0, ptr %229
  %230 = add i64 %209, 1
  %231 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1848)
  %232 = icmp slt i64 %230, %231
  br i1 %232, label %205, label %233
233:                                              ; preds = %224, %173
  %234 = phi ptr [ %174, %173 ], [ %225, %224 ]
  %235 = phi i64 [ %175, %173 ], [ %227, %224 ]
  %236 = phi i64 [ %176, %173 ], [ %226, %224 ]
  %237 = tail call ptr @calloc(i64 1024, i64 8)
  %238 = icmp eq ptr %237, null
  br i1 %238, label %239, label %240
239:                                              ; preds = %233
  call void @.gc.markASlice.cold.5()
  unreachable
240:                                              ; preds = %233
  store ptr %237, ptr %2
  %241 = getelementptr i8, ptr %2, i64 8
  store i64 1024, ptr %241
  %242 = getelementptr i8, ptr %2, i64 16
  store i64 0, ptr %242
  %243 = icmp eq i64 %235, 0
  br i1 %243, label %486, label %244
244:                                              ; preds = %240, %479
  %245 = phi ptr [ %480, %479 ], [ %234, %240 ]
  %246 = phi i64 [ %482, %479 ], [ %236, %240 ]
  %247 = phi i64 [ %481, %479 ], [ %235, %240 ]
  %248 = phi i64 [ %484, %479 ], [ 0, %240 ]
  %249 = add i64 %247, -1
  %250 = getelementptr %struct.GcGrey, ptr %245, i64 %249
  %251 = load i64, ptr %250
  %252 = icmp eq i64 %251, 0
  %253 = and i64 %251, 15
  %254 = icmp ne i64 %253, 0
  %255 = or i1 %252, %254
  %256 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %257 = icmp ult i64 %251, %256
  %258 = select i1 %255, i1 true, i1 %257
  %259 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %260 = icmp ugt i64 %251, %259
  %261 = select i1 %258, i1 true, i1 %260
  br i1 %261, label %392, label %262
262:                                              ; preds = %244
  %263 = and i64 %251, -65536
  %264 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1712)
  %265 = icmp eq i64 %264, 0
  br i1 %265, label %294, label %266
266:                                              ; preds = %262
  %267 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1704)
  %268 = lshr i64 %263, 4
  %269 = mul i64 %268, -7046029254386353131
  %270 = lshr i64 %269, 20
  %271 = add i64 %264, 17592186044415
  %272 = and i64 %271, %270
  %273 = getelementptr i64, ptr %267, i64 %272
  %274 = load i64, ptr %273
  %275 = icmp eq i64 %274, 0
  %276 = icmp eq i64 %274, %263
  %277 = or i1 %275, %276
  br i1 %277, label %289, label %278
278:                                              ; preds = %266
  %279 = add i64 %264, -1
  br label %280
280:                                              ; preds = %280, %278
  %281 = phi i64 [ %272, %278 ], [ %283, %280 ]
  %282 = add i64 %281, 1
  %283 = and i64 %282, %279
  %284 = getelementptr i64, ptr %267, i64 %283
  %285 = load i64, ptr %284
  %286 = icmp eq i64 %285, 0
  %287 = icmp eq i64 %285, %263
  %288 = or i1 %286, %287
  br i1 %288, label %289, label %280
289:                                              ; preds = %280, %266
  %290 = phi i64 [ %272, %266 ], [ %283, %280 ]
  %291 = getelementptr i64, ptr %267, i64 %290
  %292 = load i64, ptr %291
  %293 = icmp ne i64 %292, %263
  br label %294
294:                                              ; preds = %289, %262
  %295 = phi i1 [ true, %262 ], [ %293, %289 ]
  %296 = inttoptr i64 %263 to ptr
  %297 = icmp eq i64 %263, 0
  %298 = or i1 %297, %295
  br i1 %298, label %328, label %299
299:                                              ; preds = %294
  %300 = or i64 %263, 1072
  %301 = icmp ult i64 %251, %300
  br i1 %301, label %328, label %302
302:                                              ; preds = %299
  %303 = sub i64 %251, %300
  %304 = getelementptr i8, ptr %296, i64 8
  %305 = load i32, ptr %304
  %306 = zext i32 %305 to i64
  %307 = freeze i64 %303
  %308 = freeze i64 %306
  %309 = udiv i64 %307, %308
  %310 = mul i64 %309, %308
  %311 = sub i64 %307, %310
  %312 = icmp eq i64 %311, 0
  br i1 %312, label %313, label %328
313:                                              ; preds = %302
  %314 = getelementptr i8, ptr %296, i64 12
  %315 = load i32, ptr %314
  %316 = zext i32 %315 to i64
  %317 = icmp ult i64 %309, %316
  br i1 %317, label %318, label %328
318:                                              ; preds = %313
  %319 = getelementptr i8, ptr %296, i64 40
  %320 = lshr i64 %309, 6
  %321 = getelementptr i64, ptr %319, i64 %320
  %322 = load i64, ptr %321
  %323 = and i64 %309, 63
  %324 = shl i64 1, %323
  %325 = and i64 %322, %324
  %326 = icmp eq i64 %325, 0
  %327 = select i1 %326, i64 -1, i64 %309
  br label %328
328:                                              ; preds = %318, %313, %302, %299, %294
  %329 = phi i64 [ -1, %294 ], [ -1, %302 ], [ -1, %299 ], [ -1, %313 ], [ %327, %318 ]
  %330 = icmp sgt i64 %329, -1
  br i1 %330, label %331, label %347
331:                                              ; preds = %328
  %332 = getelementptr i8, ptr %296, i64 8
  %333 = load i32, ptr %332
  %334 = zext i32 %333 to i64
  %335 = getelementptr i8, ptr %296, i64 16
  %336 = load i32, ptr %335
  %337 = zext i32 %336 to i64
  %338 = getelementptr i8, ptr %296, i64 552
  %339 = lshr i64 %329, 6
  %340 = getelementptr i64, ptr %338, i64 %339
  %341 = load i64, ptr %340
  %342 = and i64 %329, 63
  %343 = lshr i64 %341, %342
  %344 = shl i64 %343, 32
  %345 = and i64 %344, 4294967296
  %346 = or i64 %345, %337
  br label %392
347:                                              ; preds = %328
  br i1 %298, label %348, label %392
348:                                              ; preds = %347
  %349 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1760)
  %350 = icmp eq i64 %349, 0
  br i1 %350, label %392, label %351
351:                                              ; preds = %348
  %352 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1752)
  %353 = lshr i64 %251, 4
  %354 = mul i64 %353, -7046029254386353131
  %355 = lshr i64 %354, 20
  %356 = add i64 %349, 17592186044415
  %357 = and i64 %356, %355
  %358 = getelementptr i64, ptr %352, i64 %357
  %359 = load i64, ptr %358
  %360 = icmp eq i64 %359, 0
  %361 = icmp eq i64 %359, %251
  %362 = or i1 %360, %361
  br i1 %362, label %374, label %363
363:                                              ; preds = %351
  %364 = add i64 %349, -1
  br label %365
365:                                              ; preds = %365, %363
  %366 = phi i64 [ %357, %363 ], [ %368, %365 ]
  %367 = add i64 %366, 1
  %368 = and i64 %367, %364
  %369 = getelementptr i64, ptr %352, i64 %368
  %370 = load i64, ptr %369
  %371 = icmp eq i64 %370, 0
  %372 = icmp eq i64 %370, %251
  %373 = or i1 %371, %372
  br i1 %373, label %374, label %365
374:                                              ; preds = %365, %351
  %375 = phi i64 [ %357, %351 ], [ %368, %365 ]
  %376 = getelementptr i64, ptr %352, i64 %375
  %377 = load i64, ptr %376
  %378 = icmp eq i64 %377, %251
  br i1 %378, label %379, label %392
379:                                              ; preds = %374
  %380 = add i64 %251, -32
  %381 = inttoptr i64 %380 to ptr
  %382 = getelementptr i8, ptr %381, i64 16
  %383 = load i64, ptr %382
  %384 = getelementptr i8, ptr %381, i64 24
  %385 = load i32, ptr %384
  %386 = zext i32 %385 to i64
  %387 = getelementptr i8, ptr %381, i64 28
  %388 = load i32, ptr %387
  %389 = zext i32 %388 to i64
  %390 = shl i64 %389, 32
  %391 = or i64 %390, %386
  br label %392
392:                                              ; preds = %379, %374, %348, %347, %331, %244
  %393 = phi i64 [ 0, %244 ], [ %334, %331 ], [ %383, %379 ], [ 0, %374 ], [ 0, %347 ], [ 0, %348 ]
  %394 = phi i64 [ 0, %244 ], [ %346, %331 ], [ %391, %379 ], [ 0, %374 ], [ 0, %347 ], [ 0, %348 ]
  %395 = icmp eq i64 %393, 0
  br i1 %395, label %479, label %396
396:                                              ; preds = %392
  %397 = load i64, ptr %241
  %398 = icmp eq i64 %397, 0
  br i1 %398, label %427, label %399
399:                                              ; preds = %396
  %400 = load ptr, ptr %2
  %401 = lshr i64 %251, 4
  %402 = mul i64 %401, -7046029254386353131
  %403 = lshr i64 %402, 20
  %404 = add i64 %397, 17592186044415
  %405 = and i64 %404, %403
  %406 = getelementptr i64, ptr %400, i64 %405
  %407 = load i64, ptr %406
  %408 = icmp eq i64 %407, 0
  %409 = icmp eq i64 %407, %251
  %410 = or i1 %408, %409
  br i1 %410, label %422, label %411
411:                                              ; preds = %399
  %412 = add i64 %397, -1
  br label %413
413:                                              ; preds = %413, %411
  %414 = phi i64 [ %405, %411 ], [ %416, %413 ]
  %415 = add i64 %414, 1
  %416 = and i64 %415, %412
  %417 = getelementptr i64, ptr %400, i64 %416
  %418 = load i64, ptr %417
  %419 = icmp eq i64 %418, 0
  %420 = icmp eq i64 %418, %251
  %421 = or i1 %419, %420
  br i1 %421, label %422, label %413
422:                                              ; preds = %413, %399
  %423 = phi i64 [ %405, %399 ], [ %416, %413 ]
  %424 = getelementptr i64, ptr %400, i64 %423
  %425 = load i64, ptr %424
  %426 = icmp eq i64 %425, %251
  br i1 %426, label %479, label %427
427:                                              ; preds = %422, %396
  call fastcc void @.gc.setAdd(ptr %2, i64 %251)
  %428 = and i64 %394, 4294967295
  %429 = icmp eq i64 %428, 1
  br i1 %429, label %430, label %473
430:                                              ; preds = %427
  %431 = inttoptr i64 %251 to ptr
  %432 = lshr i64 %393, 3
  %433 = icmp ult i64 %393, 8
  br i1 %433, label %473, label %434
434:                                              ; preds = %430, %467
  %435 = phi ptr [ %468, %467 ], [ %245, %430 ]
  %436 = phi i64 [ %469, %467 ], [ %249, %430 ]
  %437 = phi i64 [ %470, %467 ], [ %246, %430 ]
  %438 = phi i64 [ %471, %467 ], [ 0, %430 ]
  %439 = getelementptr i64, ptr %431, i64 %438
  %440 = load i64, ptr %439
  %441 = icmp eq i64 %440, 0
  %442 = and i64 %440, 15
  %443 = icmp ne i64 %442, 0
  %444 = or i1 %441, %443
  %445 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1800)
  %446 = icmp ult i64 %440, %445
  %447 = select i1 %444, i1 true, i1 %446
  %448 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1808)
  %449 = icmp ugt i64 %440, %448
  %450 = select i1 %447, i1 true, i1 %449
  br i1 %450, label %467, label %451
451:                                              ; preds = %434
  %452 = icmp ult i64 %436, %437
  br i1 %452, label %461, label %453
453:                                              ; preds = %451
  %454 = icmp eq i64 %437, 0
  %455 = shl i64 %437, 1
  %456 = select i1 %454, i64 256, i64 %455
  %457 = shl i64 %456, 4
  %458 = tail call ptr @realloc(ptr %435, i64 %457)
  %459 = icmp eq ptr %458, null
  br i1 %459, label %460, label %461
460:                                              ; preds = %453
  call void @.gc.markASlice.cold.4()
  unreachable
461:                                              ; preds = %453, %451
  %462 = phi ptr [ %435, %451 ], [ %458, %453 ]
  %463 = phi i64 [ %437, %451 ], [ %456, %453 ]
  %464 = add i64 %436, 1
  %465 = getelementptr %struct.GcGrey, ptr %462, i64 %436
  store i64 %440, ptr %465
  %466 = getelementptr i8, ptr %465, i64 8
  store i64 0, ptr %466
  br label %467
467:                                              ; preds = %461, %434
  %468 = phi ptr [ %435, %434 ], [ %462, %461 ]
  %469 = phi i64 [ %436, %434 ], [ %464, %461 ]
  %470 = phi i64 [ %437, %434 ], [ %463, %461 ]
  %471 = add i64 %438, 1
  %472 = icmp eq i64 %471, %432
  br i1 %472, label %473, label %434
473:                                              ; preds = %467, %430, %427
  %474 = phi ptr [ %245, %430 ], [ %245, %427 ], [ %468, %467 ]
  %475 = phi i64 [ %249, %430 ], [ %249, %427 ], [ %469, %467 ]
  %476 = phi i64 [ %246, %430 ], [ %246, %427 ], [ %470, %467 ]
  %477 = icmp ult i64 %394, 4294967296
  %478 = zext i1 %477 to i64
  br label %479
479:                                              ; preds = %473, %422, %392
  %480 = phi ptr [ %245, %392 ], [ %474, %473 ], [ %245, %422 ]
  %481 = phi i64 [ %249, %392 ], [ %475, %473 ], [ %249, %422 ]
  %482 = phi i64 [ %246, %392 ], [ %476, %473 ], [ %246, %422 ]
  %483 = phi i64 [ 0, %392 ], [ %478, %473 ], [ 0, %422 ]
  %484 = add i64 %483, %248
  %485 = icmp eq i64 %481, 0
  br i1 %485, label %486, label %244
486:                                              ; preds = %479, %240
  %487 = phi ptr [ %234, %240 ], [ %480, %479 ]
  %488 = phi i64 [ 0, %240 ], [ %484, %479 ]
  %489 = load ptr, ptr %2
  tail call void @free(ptr %489)
  tail call void @free(ptr %487)
  %490 = icmp eq i64 %488, 0
  br i1 %490, label %492, label %491
491:                                              ; preds = %486
  tail call fastcc void @.gc.reportMissed(i64 %488)
  unreachable
492:                                              ; preds = %151, %486
  store i32 0, ptr @gc_marking
  store i32 2, ptr getelementptr (i8, ptr @gc_state, i64 4)
  %493 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 24)
  %494 = add i32 %493, 1
  store i32 %494, ptr getelementptr (i8, ptr @gc_state, i64 24)
  store i64 0, ptr getelementptr (i8, ptr @gc_state, i64 1896)
  store i32 0, ptr getelementptr (i8, ptr @gc_state, i64 1912)
  store ptr null, ptr getelementptr (i8, ptr @gc_state, i64 1920)
  %495 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  store ptr %495, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  br label %496
496:                                              ; preds = %492, %149
  ret void
}
define internal fastcc void @.gc.sweepASlice(i64 %0) {
  %2 = alloca [160 x i8]
  %3 = icmp ne i64 %0, 0
  %4 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 1912)
  %5 = icmp slt i32 %4, 48
  %6 = select i1 %3, i1 %5, i1 false
  br i1 %6, label %13, label %7
7:                                                ; preds = %112, %1
  %8 = phi i64 [ 0, %1 ], [ %114, %112 ]
  %9 = icmp uge i64 %8, %0
  %10 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %11 = icmp eq ptr %10, null
  %12 = select i1 %9, i1 true, i1 %11
  br i1 %12, label %177, label %119
13:                                               ; preds = %1, %112
  %14 = phi i32 [ %116, %112 ], [ %4, %1 ]
  %15 = phi i64 [ %114, %112 ], [ 0, %1 ]
  %16 = freeze i32 %14
  %17 = sdiv i32 %16, 24
  %18 = sext i32 %17 to i64
  %19 = mul i32 %17, 24
  %20 = sub i32 %16, %19
  %21 = sext i32 %20 to i64
  %22 = getelementptr [2 x [24 x %struct.GcClass]], ptr getelementptr (i8, ptr @gc_state, i64 32), i64 0, i64 %18, i64 %21
  %23 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1920)
  %24 = icmp eq ptr %23, null
  %25 = select i1 %24, ptr %22, ptr %23
  %26 = load ptr, ptr %25
  %27 = icmp eq ptr %26, null
  br i1 %27, label %33, label %28
28:                                               ; preds = %13
  %29 = getelementptr i8, ptr %26, i64 32
  %30 = load i32, ptr %29
  %31 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 24)
  %32 = icmp eq i32 %30, %31
  br i1 %32, label %33, label %37
33:                                               ; preds = %28, %13
  %34 = load ptr, ptr %22
  %35 = getelementptr i8, ptr %22, i64 16
  store ptr %34, ptr %35
  %36 = add i32 %14, 1
  store i32 %36, ptr getelementptr (i8, ptr @gc_state, i64 1912)
  store ptr null, ptr getelementptr (i8, ptr @gc_state, i64 1920)
  br label %112
37:                                               ; preds = %28
  %38 = getelementptr i8, ptr %26, i64 24
  %39 = load i32, ptr %38
  %40 = icmp eq i32 %39, 0
  br i1 %40, label %74, label %41
41:                                               ; preds = %37
  %42 = getelementptr i8, ptr %26, i64 40
  %43 = getelementptr i8, ptr %26, i64 552
  %44 = getelementptr i8, ptr %26, i64 8
  br label %45
45:                                               ; preds = %45, %41
  %46 = phi i64 [ 0, %41 ], [ %70, %45 ]
  %47 = phi i32 [ 0, %41 ], [ %69, %45 ]
  %48 = getelementptr [64 x i64], ptr %42, i64 0, i64 %46
  %49 = load i64, ptr %48
  %50 = getelementptr [64 x i64], ptr %43, i64 0, i64 %46
  %51 = load i64, ptr %50
  %52 = xor i64 %51, -1
  %53 = and i64 %49, %52
  %54 = tail call i64 @llvm.ctpop.i64(i64 %53)
  %55 = load i32, ptr %44
  %56 = zext i32 %55 to i64
  %57 = mul i64 %54, %56
  %58 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9936)
  %59 = add i64 %58, %54
  store i64 %59, ptr getelementptr (i8, ptr @gc_state, i64 9936)
  %60 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9944)
  %61 = add i64 %60, %57
  store i64 %61, ptr getelementptr (i8, ptr @gc_state, i64 9944)
  %62 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %63 = sub i64 %62, %54
  store i64 %63, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %64 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %65 = sub i64 %64, %57
  store i64 %65, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %66 = load i64, ptr %50
  store i64 %66, ptr %48
  store i64 0, ptr %50
  %67 = tail call i64 @llvm.ctpop.i64(i64 %66)
  %68 = trunc i64 %67 to i32
  %69 = add i32 %47, %68
  %70 = add i64 %46, 1
  %71 = load i32, ptr %38
  %72 = zext i32 %71 to i64
  %73 = icmp ult i64 %70, %72
  br i1 %73, label %45, label %74
74:                                               ; preds = %45, %37
  %75 = phi i32 [ 0, %37 ], [ %69, %45 ]
  %76 = getelementptr i8, ptr %26, i64 20
  store i32 %75, ptr %76
  %77 = getelementptr i8, ptr %26, i64 28
  store i32 0, ptr %77
  %78 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 24)
  store i32 %78, ptr %29
  %79 = zext i32 %75 to i64
  %80 = getelementptr i8, ptr %26, i64 8
  %81 = load i32, ptr %80
  %82 = zext i32 %81 to i64
  %83 = mul i64 %82, %79
  %84 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1896)
  %85 = add i64 %83, %84
  store i64 %85, ptr getelementptr (i8, ptr @gc_state, i64 1896)
  %86 = icmp eq i32 %75, 0
  br i1 %86, label %87, label %107
87:                                               ; preds = %74
  %88 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1920)
  %89 = icmp eq ptr %88, null
  %90 = load ptr, ptr %26
  br i1 %89, label %92, label %91
91:                                               ; preds = %87
  store ptr %90, ptr %88
  br label %93
92:                                               ; preds = %87
  store ptr %90, ptr %22
  br label %93
93:                                               ; preds = %92, %91
  %94 = getelementptr i8, ptr %22, i64 8
  %95 = load ptr, ptr %94
  %96 = icmp eq ptr %95, %26
  br i1 %96, label %97, label %98
97:                                               ; preds = %93
  store ptr %88, ptr %94
  br label %98
98:                                               ; preds = %97, %93
  %99 = getelementptr i8, ptr %22, i64 16
  %100 = load ptr, ptr %99
  %101 = icmp eq ptr %100, %26
  br i1 %101, label %102, label %103
102:                                              ; preds = %98
  store ptr %88, ptr %99
  br label %103
103:                                              ; preds = %102, %98
  %104 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  store ptr %104, ptr %26
  store ptr %26, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  %105 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  %106 = add i64 %105, 1
  store i64 %106, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  br label %108
107:                                              ; preds = %74
  store ptr %26, ptr getelementptr (i8, ptr @gc_state, i64 1920)
  br label %108
108:                                              ; preds = %107, %103
  %109 = load i32, ptr %38
  %110 = add i32 %109, 1
  %111 = zext i32 %110 to i64
  br label %112
112:                                              ; preds = %33, %108
  %113 = phi i64 [ 1, %33 ], [ %111, %108 ]
  %114 = add i64 %113, %15
  %115 = icmp ult i64 %114, %0
  %116 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 1912)
  %117 = icmp slt i32 %116, 48
  %118 = select i1 %115, i1 %117, i1 false
  br i1 %118, label %13, label %7
119:                                              ; preds = %7, %171
  %120 = phi ptr [ %174, %171 ], [ %10, %7 ]
  %121 = phi i64 [ %172, %171 ], [ %8, %7 ]
  %122 = getelementptr i8, ptr %120, i64 8
  %123 = load ptr, ptr %122
  store ptr %123, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %124 = getelementptr i8, ptr %120, i64 28
  %125 = load i32, ptr %124
  %126 = icmp eq i32 %125, 0
  br i1 %126, label %133, label %127
127:                                              ; preds = %119
  store i32 0, ptr %124
  %128 = getelementptr i8, ptr %120, i64 16
  %129 = load i64, ptr %128
  %130 = add i64 %129, 32
  %131 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1896)
  %132 = add i64 %130, %131
  store i64 %132, ptr getelementptr (i8, ptr @gc_state, i64 1896)
  br label %171
133:                                              ; preds = %119
  %134 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9936)
  %135 = add i64 %134, 1
  store i64 %135, ptr getelementptr (i8, ptr @gc_state, i64 9936)
  %136 = getelementptr i8, ptr %120, i64 16
  %137 = load i64, ptr %136
  %138 = add i64 %137, 32
  %139 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 9944)
  %140 = add i64 %138, %139
  store i64 %140, ptr getelementptr (i8, ptr @gc_state, i64 9944)
  %141 = getelementptr i8, ptr %120, i64 32
  %142 = ptrtoint ptr %141 to i64
  %143 = add i64 %142, -32
  %144 = inttoptr i64 %143 to ptr
  tail call fastcc void @.gc.setRemove(ptr getelementptr (i8, ptr @gc_state, i64 1752), i64 %142)
  %145 = load ptr, ptr %144
  %146 = icmp eq ptr %145, null
  %147 = getelementptr i8, ptr %144, i64 8
  %148 = load ptr, ptr %147
  br i1 %146, label %151, label %149
149:                                              ; preds = %133
  %150 = getelementptr i8, ptr %145, i64 8
  store ptr %148, ptr %150
  br label %152
151:                                              ; preds = %133
  store ptr %148, ptr getelementptr (i8, ptr @gc_state, i64 1776)
  br label %152
152:                                              ; preds = %151, %149
  %153 = getelementptr i8, ptr %144, i64 8
  %154 = load ptr, ptr %153
  %155 = icmp eq ptr %154, null
  br i1 %155, label %158, label %156
156:                                              ; preds = %152
  %157 = load ptr, ptr %144
  store ptr %157, ptr %154
  br label %158
158:                                              ; preds = %156, %152
  %159 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %160 = icmp eq ptr %159, %144
  br i1 %160, label %161, label %163
161:                                              ; preds = %158
  %162 = load ptr, ptr %153
  store ptr %162, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  br label %163
163:                                              ; preds = %161, %158
  %164 = getelementptr i8, ptr %144, i64 16
  %165 = load i64, ptr %164
  %166 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %167 = sub i64 %166, %165
  %168 = add i64 %167, -32
  store i64 %168, ptr getelementptr (i8, ptr @gc_state, i64 1784)
  %169 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  %170 = add i64 %169, -1
  store i64 %170, ptr getelementptr (i8, ptr @gc_state, i64 1792)
  tail call void @free(ptr %144)
  br label %171
171:                                              ; preds = %127, %163
  %172 = add i64 %121, 1
  %173 = icmp uge i64 %172, %0
  %174 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1928)
  %175 = icmp eq ptr %174, null
  %176 = select i1 %173, i1 true, i1 %175
  br i1 %176, label %177, label %119
177:                                              ; preds = %171, %7
  %178 = phi i1 [ %11, %7 ], [ %175, %171 ]
  %179 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 1912)
  %180 = icmp eq i32 %179, 48
  %181 = select i1 %180, i1 %178, i1 false
  br i1 %181, label %182, label %217
182:                                              ; preds = %177
  %183 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1744)
  %184 = icmp eq i64 %183, 0
  %185 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  %186 = icmp eq ptr %185, null
  %187 = select i1 %184, i1 true, i1 %186
  br i1 %187, label %200, label %188
188:                                              ; preds = %182, %188
  %189 = phi ptr [ %197, %188 ], [ %185, %182 ]
  %190 = phi i64 [ %195, %188 ], [ %183, %182 ]
  %191 = load ptr, ptr %189
  store ptr %191, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  %192 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  %193 = add i64 %192, -1
  store i64 %193, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  %194 = ptrtoint ptr %189 to i64
  tail call fastcc void @.gc.setRemove(ptr getelementptr (i8, ptr @gc_state, i64 1704), i64 %194)
  tail call void @free(ptr %189)
  %195 = add i64 %190, -1
  %196 = icmp eq i64 %195, 0
  %197 = load ptr, ptr getelementptr (i8, ptr @gc_state, i64 1728)
  %198 = icmp eq ptr %197, null
  %199 = select i1 %196, i1 true, i1 %198
  br i1 %199, label %200, label %188
200:                                              ; preds = %188, %182
  %201 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1736)
  store i64 %201, ptr getelementptr (i8, ptr @gc_state, i64 1744)
  %202 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1896)
  %203 = shl i64 %202, 1
  %204 = tail call i64 @llvm.umax.i64(i64 %203, i64 8388608)
  %205 = load i64, ptr getelementptr (i8, ptr @gc_state, i64 1832)
  %206 = tail call i64 @llvm.umin.i64(i64 %204, i64 %205)
  %207 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 12)
  %208 = icmp eq i32 %207, 0
  %209 = load i32, ptr getelementptr (i8, ptr @gc_state, i64 16)
  %210 = icmp eq i32 %209, 0
  %211 = select i1 %208, i1 %210, i1 false
  %212 = select i1 %211, i64 %206, i64 0
  store i64 %212, ptr getelementptr (i8, ptr @gc_state, i64 1824)
  %213 = icmp ugt i64 %202, %205
  br i1 %213, label %214, label %216
214:                                              ; preds = %200
  %215 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %2, i64 160, ptr @.gc_str.1, i64 %202, i64 %205)
  call fastcc void @.gc.stopTheProgram(ptr %2)
  unreachable
216:                                              ; preds = %200
  store i32 0, ptr getelementptr (i8, ptr @gc_state, i64 4)
  br label %217
217:                                              ; preds = %216, %177
  ret void
}
define internal fastcc void @.gc.reportMissed(i64 %0) {
  %2 = alloca [200 x i8]
  %3 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %2, i64 200, ptr @.gc_str, i64 %0)
  call fastcc void @.gc.stopTheProgram(ptr %2)
  unreachable
}
define internal fastcc void @.gc.setAdd(ptr %0, i64 %1) {
  %3 = getelementptr i8, ptr %0, i64 8
  %4 = load i64, ptr %3
  %5 = icmp eq i64 %4, 0
  br i1 %5, label %6, label %12
6:                                                ; preds = %2
  %7 = tail call ptr @calloc(i64 1024, i64 8)
  %8 = icmp eq ptr %7, null
  br i1 %8, label %9, label %10
9:                                                ; preds = %6
  call void @.gc.setAdd.cold.2()
  unreachable
10:                                               ; preds = %6
  store ptr %7, ptr %0
  store i64 1024, ptr %3
  %11 = getelementptr i8, ptr %0, i64 16
  store i64 0, ptr %11
  br label %12
12:                                               ; preds = %10, %2
  %13 = getelementptr i8, ptr %0, i64 16
  %14 = load i64, ptr %13
  %15 = shl i64 %14, 2
  %16 = add i64 %15, 4
  %17 = load i64, ptr %3
  %18 = mul i64 %17, 3
  %19 = icmp ugt i64 %16, %18
  br i1 %19, label %20, label %67
20:                                               ; preds = %12
  %21 = shl i64 %17, 1
  %22 = tail call ptr @calloc(i64 %21, i64 8)
  %23 = icmp eq ptr %22, null
  br i1 %23, label %30, label %24
24:                                               ; preds = %20
  %25 = icmp eq i64 %17, 0
  br i1 %25, label %64, label %26
26:                                               ; preds = %24
  %27 = load ptr, ptr %0
  %28 = add i64 %21, 17592186044415
  %29 = add i64 %21, -1
  br label %31
30:                                               ; preds = %20
  call void @.gc.setAdd.cold.1()
  unreachable
31:                                               ; preds = %60, %26
  %32 = phi i64 [ 0, %26 ], [ %62, %60 ]
  %33 = phi i64 [ 0, %26 ], [ %61, %60 ]
  %34 = getelementptr i64, ptr %27, i64 %32
  %35 = load i64, ptr %34
  %36 = icmp eq i64 %35, 0
  br i1 %36, label %60, label %37
37:                                               ; preds = %31
  %38 = lshr i64 %35, 4
  %39 = mul i64 %38, -7046029254386353131
  %40 = lshr i64 %39, 20
  %41 = and i64 %40, %28
  %42 = getelementptr i64, ptr %22, i64 %41
  %43 = load i64, ptr %42
  %44 = icmp eq i64 %43, 0
  %45 = icmp eq i64 %43, %35
  %46 = or i1 %44, %45
  br i1 %46, label %56, label %47
47:                                               ; preds = %37, %47
  %48 = phi i64 [ %50, %47 ], [ %41, %37 ]
  %49 = add i64 %48, 1
  %50 = and i64 %49, %29
  %51 = getelementptr i64, ptr %22, i64 %50
  %52 = load i64, ptr %51
  %53 = icmp eq i64 %52, 0
  %54 = icmp eq i64 %52, %35
  %55 = or i1 %53, %54
  br i1 %55, label %56, label %47
56:                                               ; preds = %47, %37
  %57 = phi i64 [ %41, %37 ], [ %50, %47 ]
  %58 = getelementptr i64, ptr %22, i64 %57
  store i64 %35, ptr %58
  %59 = add i64 %33, 1
  br label %60
60:                                               ; preds = %56, %31
  %61 = phi i64 [ %33, %31 ], [ %59, %56 ]
  %62 = add i64 %32, 1
  %63 = icmp eq i64 %62, %17
  br i1 %63, label %64, label %31
64:                                               ; preds = %60, %24
  %65 = phi i64 [ 0, %24 ], [ %61, %60 ]
  %66 = load ptr, ptr %0
  tail call void @free(ptr %66)
  store ptr %22, ptr %0
  store i64 %21, ptr %3
  store i64 %65, ptr %13
  br label %67
67:                                               ; preds = %64, %12
  %68 = load ptr, ptr %0
  %69 = lshr i64 %1, 4
  %70 = mul i64 %69, -7046029254386353131
  %71 = lshr i64 %70, 20
  %72 = load i64, ptr %3
  %73 = add i64 %72, 17592186044415
  %74 = and i64 %73, %71
  %75 = getelementptr i64, ptr %68, i64 %74
  %76 = load i64, ptr %75
  %77 = icmp eq i64 %76, 0
  %78 = icmp eq i64 %76, %1
  %79 = or i1 %77, %78
  br i1 %79, label %91, label %80
80:                                               ; preds = %67
  %81 = add i64 %72, -1
  br label %82
82:                                               ; preds = %82, %80
  %83 = phi i64 [ %74, %80 ], [ %85, %82 ]
  %84 = add i64 %83, 1
  %85 = and i64 %84, %81
  %86 = getelementptr i64, ptr %68, i64 %85
  %87 = load i64, ptr %86
  %88 = icmp eq i64 %87, 0
  %89 = icmp eq i64 %87, %1
  %90 = or i1 %88, %89
  br i1 %90, label %91, label %82
91:                                               ; preds = %82, %67
  %92 = phi i64 [ %74, %67 ], [ %85, %82 ]
  %93 = getelementptr i64, ptr %68, i64 %92
  store i64 %1, ptr %93
  %94 = load i64, ptr %13
  %95 = add i64 %94, 1
  store i64 %95, ptr %13
  ret void
}
define internal fastcc void @.gc.stopTheProgram(ptr %0) {
  %2 = tail call i32 @fflush(ptr null)
  %3 = tail call i64 @strlen(ptr %0)
  %4 = tail call i64 @write(i32 2, ptr %0, i64 %3)
  tail call void @llvm.trap()
  unreachable
}
define internal fastcc void @.gc.setRemove(ptr %0, i64 %1) {
  %3 = lshr i64 %1, 4
  %4 = mul i64 %3, -7046029254386353131
  %5 = lshr i64 %4, 20
  %6 = getelementptr i8, ptr %0, i64 8
  %7 = load i64, ptr %6
  %8 = add i64 %7, 17592186044415
  %9 = and i64 %8, %5
  %10 = load ptr, ptr %0
  %11 = getelementptr i64, ptr %10, i64 %9
  %12 = load i64, ptr %11
  %13 = icmp eq i64 %12, 0
  %14 = icmp eq i64 %12, %1
  %15 = or i1 %13, %14
  br i1 %15, label %27, label %16
16:                                               ; preds = %2
  %17 = add i64 %7, -1
  br label %18
18:                                               ; preds = %18, %16
  %19 = phi i64 [ %9, %16 ], [ %21, %18 ]
  %20 = add i64 %19, 1
  %21 = and i64 %20, %17
  %22 = getelementptr i64, ptr %10, i64 %21
  %23 = load i64, ptr %22
  %24 = icmp eq i64 %23, 0
  %25 = icmp eq i64 %23, %1
  %26 = or i1 %24, %25
  br i1 %26, label %27, label %18
27:                                               ; preds = %18, %2
  %28 = phi i64 [ %9, %2 ], [ %21, %18 ]
  %29 = add i64 %28, 1
  %30 = add i64 %7, -1
  %31 = and i64 %29, %30
  %32 = getelementptr i64, ptr %10, i64 %31
  %33 = load i64, ptr %32
  %34 = icmp eq i64 %33, 0
  br i1 %34, label %67, label %35
35:                                               ; preds = %27, %57
  %36 = phi i64 [ %65, %57 ], [ %33, %27 ]
  %37 = phi ptr [ %63, %57 ], [ %10, %27 ]
  %38 = phi i64 [ %62, %57 ], [ %31, %27 ]
  %39 = phi i64 [ %58, %57 ], [ %28, %27 ]
  %40 = lshr i64 %36, 4
  %41 = mul i64 %40, -7046029254386353131
  %42 = lshr i64 %41, 20
  %43 = load i64, ptr %6
  %44 = add i64 %43, 17592186044415
  %45 = and i64 %44, %42
  %46 = icmp ugt i64 %39, %38
  %47 = icmp ugt i64 %45, %39
  %48 = icmp ule i64 %45, %38
  br i1 %46, label %51, label %49
49:                                               ; preds = %35
  %50 = and i1 %47, %48
  br label %53
51:                                               ; preds = %35
  %52 = or i1 %47, %48
  br label %53
53:                                               ; preds = %51, %49
  %54 = phi i1 [ %50, %49 ], [ %52, %51 ]
  br i1 %54, label %57, label %55
55:                                               ; preds = %53
  %56 = getelementptr i64, ptr %37, i64 %39
  store i64 %36, ptr %56
  br label %57
57:                                               ; preds = %55, %53
  %58 = phi i64 [ %39, %53 ], [ %38, %55 ]
  %59 = add i64 %38, 1
  %60 = load i64, ptr %6
  %61 = add i64 %60, -1
  %62 = and i64 %61, %59
  %63 = load ptr, ptr %0
  %64 = getelementptr i64, ptr %63, i64 %62
  %65 = load i64, ptr %64
  %66 = icmp eq i64 %65, 0
  br i1 %66, label %67, label %35
67:                                               ; preds = %57, %27
  %68 = phi i64 [ %28, %27 ], [ %58, %57 ]
  %69 = load ptr, ptr %0
  %70 = getelementptr i64, ptr %69, i64 %68
  store i64 0, ptr %70
  %71 = getelementptr i8, ptr %0, i64 16
  %72 = load i64, ptr %71
  %73 = add i64 %72, -1
  store i64 %73, ptr %71
  ret void
}
define internal void @.gc.markWord.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.markWord.cold.2() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.gc_allocate.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.newObject.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.newObject.cold.2() {
  br label %1
1:                                                ; preds = %0
  call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.gc_allocate_zeroed.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.gc_resize.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.gc_resize.cold.2() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.gc_resize.cold.3() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.markASlice.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.markASlice.cold.2() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.markASlice.cold.3() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.markASlice.cold.4() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.markASlice.cold.5() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.setAdd.cold.1() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
define internal void @.gc.setAdd.cold.2() {
  br label %1
1:                                                ; preds = %0
  tail call fastcc void @.gc.stopTheProgram(ptr @.gc_str.2)
  unreachable
}
; END GENERATED)IR";

    // The C library functions and intrinsics the collector calls, one `declare` each.
    const Vector<const char*> kCollectorDeclarations = {
      // BEGIN DECLARATIONS
      "declare ptr @malloc(i64)",
      "declare ptr @calloc(i64, i64)",
      "declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)",
      "declare ptr @realloc(ptr, i64)",
      "declare void @free(ptr)",
      "declare i32 @atexit(ptr)",
      "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)",
      "declare i32 @snprintf(ptr, i64, ptr, ...)",
      "declare i32 @fflush(ptr)",
      "declare i64 @write(i32, ptr, i64)",
      "declare i64 @strlen(ptr)",
      "declare void @llvm.trap()",
      "declare i64 @llvm.ctpop.i64(i64)",
      "declare i64 @llvm.cttz.i64(i64, i1)",
      "declare i32 @posix_memalign(ptr, i64, i64)",
      "declare i32 @timespec_get(ptr, i32)",
      "declare ptr @getenv(ptr)",
      "declare i32 @strcmp(ptr, ptr)",
      "declare i64 @strtoull(ptr, ptr, i32)",
      "declare i64 @llvm.smax.i64(i64, i64)",
      "declare i64 @llvm.umin.i64(i64, i64)",
      "declare i64 @llvm.umax.i64(i64, i64)",
      // END DECLARATIONS
    };

    // The allocation functions of a module built without the collector: malloc, calloc, realloc and free, a `Free` that frees, and a write
    // barrier that does nothing (the map helpers call it either way).
    const char* const kPlainAllocator = R"IR(define internal ptr @gc_allocate(i64 %bytes, i64 %layout) {
entry:
  %block = call ptr @malloc(i64 %bytes)
  ret ptr %block
}
define internal ptr @gc_allocate_zeroed(i64 %count, i64 %size, i64 %layout) {
entry:
  %block = call ptr @calloc(i64 %count, i64 %size)
  ret ptr %block
}
define internal ptr @gc_resize(ptr %block, i64 %bytes) {
entry:
  %moved = call ptr @realloc(ptr %block, i64 %bytes)
  ret ptr %moved
}
define internal void @gc_release(ptr %block) {
entry:
  call void @free(ptr %block)
  ret void
}
define internal void @gc_free_hint(ptr %block) {
entry:
  call void @free(ptr %block)
  ret void
}
define internal void @gc_write_barrier(ptr %word) {
entry:
  ret void
})IR";

    const Vector<const char*> kPlainDeclarations = {"declare ptr @malloc(i64)", "declare ptr @calloc(i64, i64)", "declare ptr @realloc(ptr, i64)",
        "declare void @free(ptr)"};

    void needAll(ModuleContext& context, const Vector<const char*>& lines) {
      for (const char* line : lines) {
        Need(context, line);
      }
    }

  }  // namespace

  // The module gets the collector (or the plain allocator) the first time anything in it allocates.
  void NeedCollector(ModuleContext& context) {
    if (!FirstUse(context, "collector")) {
      return;
    }

    Need(context, Collects(context) ? kCollector : kPlainAllocator);

    if (Collects(context)) {
      needAll(context, kCollectorDeclarations);
    } else {
      needAll(context, kPlainDeclarations);
    }
  }

  // A block of values of the type holds references when the values are addresses. An address is anything LLVM types as
  // `ptr`: an object, a string or a raw pointer. Numbers, booleans and vectors are data.
  Layout LayoutOf(const TypePtr& type) {
    if (type->kind == TypeKind::Void || type->kind == TypeKind::Vec) {
      return Layout::Data;
    }

    return LlvmTypeText(type) == "ptr" ? Layout::References : Layout::Data;
  }

  namespace {

    bool anyFieldHoldsReferences(const StructDef& structure) {
      for (const Field& field : structure.fields) {
        if (LayoutOf(field.type) == Layout::References) {
          return true;
        }
      }

      return false;
    }

  }  // namespace

  // A struct's block holds references when any field does (and, to be safe, when the struct is not known).
  Layout StructLayout(const FunctionEmitter& emitter, const String& name) {
    const StructDef* structure = FindStruct(ModuleOf(emitter.context_), name);

    if (structure == nullptr) {
      return Layout::References;
    }

    return anyFieldHoldsReferences(*structure) ? Layout::References : Layout::Data;
  }

  String LayoutOperand(Layout layout) {
    return layout == Layout::References ? "i64 1" : "i64 0";
  }

}  // namespace polyir::llvm
