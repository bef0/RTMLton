val filename = OS.FileSys.tmpName ()
val _ = print filename

fun testRange (start, length) =
      let
        val _ = print ("start: " ^ Int.toString(start) ^ " stop: "  ^ Int.toString(length) ^ "\n")
        val allChars = Word8Vector.tabulate(length, fn i => Word8.fromInt ((i + start) mod 256))

        val outStr = BinIO.openOut filename
        val _ = BinIO.output (outStr, allChars)
        val _ = BinIO.closeOut outStr

        val inStr = BinIO.openIn filename
        val readChars = BinIO.inputAll inStr
        val _ = BinIO.closeIn inStr

        val _ = OS.FileSys.remove filename

        fun testCharF (c, cnt) =
              let
                val readC = Word8Vector.sub(readChars, cnt)
                val _ = if c = readC then
                          ()
                        else
                          print ("Error at index: " ^ (Int.toString cnt) ^ ": " ^
                                 (Word8.toString c) ^ " <> " ^ (Word8.toString readC) ^ "\n")
              in
                cnt + 1
              end

        val _ = Word8Vector.foldl testCharF 0 allChars
      in
        ()
      end

(*
tabulate (n, f)
returns a list of length n equal to [f(0), f(1), ..., f(n-1)], created from left to right. 
*)

val _ = print "basic test of writing and reading back all characters starts\n"
val _ = testRange (0, 256)
val _ = print "basic test of writing and reading back all characters done\n"

(* this next statement eventually exhausts memory *)
val _ = List.tabulate(256, fn i => List.tabulate(257, fn i2 => testRange (i, i2)))
val _ = print "test of writing files of all possible characters in strings of lengths 0-256 finished\n"
val _ = List.tabulate(6, fn i => List.tabulate(5000, fn i2 => testRange (i, i2)))

val _ = print "test finished\n"
