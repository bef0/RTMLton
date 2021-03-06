open MLton.PrimThread

local 
        fun e() = print "In function e()\n"
        val a = ref NONE

        val () = MLton.PrimThread.PThread.copyCurrent()
        val sp = MLton.PrimThread.PThread.savedPre()
				
        (* this should show the thread is in queue #0 *)

        val _ = let in print "Queue before setting priority:\n" ; displayThreadQueue(0) end
        val p = setPriority(sp, 5)

        (* this should show the thread moved to queue #5 *)

        val _ = let in print "\nQueue after setting priority:\n"; displayThreadQueue(0) end
        val p = getPriority(sp)
        val _ = print ("Priority is: " ^ Int.toString(p) ^ "\n")
        (*val r = setRunnable(sp)
        val _ = print ("Runnable said: " ^ Int.toString(r) ^ "\n")
        *)

in
	(*
	so.. parent and child get a copy of the above stack
	then come down here, and initially !a is NONE, and only
	parent is running (bc child threads are created in a 
	suspended state until u call setRUnnable... 
	then we expect parent to set 'a' to the func ref
	and tell child to start running, then parent goes and
	does other stuff
	child comes alive, and sees 'a' is SOME, so it calls it
	
	case !a in 
	NONE :
		a := !e
		setRunnable(sp)
	SOME : 
		a ; die "..."
		*)
		
	(* if we are in the thread, then call !a() otherwise dont *)
end

val rec delay =
   fn 0 => ()
    | n => delay (n - 1)
    
val rec loop =
   fn 0 => ()
    | n => (delay 50000000; loop (n - 1))

val _ = loop 100

val _ = print "Done\n"
