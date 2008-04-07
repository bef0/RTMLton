(* Copyright (C) 2008 Henry Cejtin, Matthew Fluet, Suresh
 *    Jagannathan, and Stephen Weeks.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 *)

structure Posix =
   struct
      structure FileDesc = MkAbsRepEq(type rep = C_Fd.t)
      structure GId = MkAbsRepEq(type rep = C_GId.t)
      structure PId = MkAbsRepEq(type rep = C_PId.t)
      structure Signal = MkAbsRepEq(type rep = C_Signal.t)
      structure UId = MkAbsRepEq(type rep = C_UId.t)
   end

structure PrePosix = Posix