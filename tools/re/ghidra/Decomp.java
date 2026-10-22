// Ghidra headless post-script: decompile the functions named in the script arguments
// ("label=0xADDR" from `nm`, or a symbol name) into <outdir>/<label>.c.
// Output is RE material for build/re only; never commit it.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class Decomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String out = a[0];
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        for (int i = 1; i < a.length; i++) {
            String want = a[i];
            Function hit = null;
            String label = want;
            if (want.contains("=")) { label = want.substring(0, want.indexOf('=')); want = want.substring(want.indexOf('=') + 1); }
            if (want.startsWith("0x")) {
                ghidra.program.model.address.Address ad = toAddr(want);
                hit = getFunctionAt(ad);
                if (hit == null) { disassemble(ad); hit = createFunction(ad, null); }
            }
            if (hit == null) for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                String n = f.getName(true);
                if (n.equals(want) || f.getName().equals(want)) { hit = f; break; }
            }
            if (hit == null) {
                // no function object yet (no analysis): create one at the symbol
                for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
                    if (s.getName(true).equals(want) || s.getName().equals(want)) {
                        hit = createFunction(s.getAddress(), null);
                        if (hit == null) hit = getFunctionAt(s.getAddress());
                        break;
                    }
                }
            }
            if (hit == null) { println("NOT FOUND " + want); continue; }
            DecompileResults r = d.decompileFunction(hit, 120, monitor);
            String c = r.decompileCompleted() ? r.getDecompiledFunction().getC() : ("// failed: " + r.getErrorMessage());
            String fn = label.replaceAll("[^A-Za-z0-9_]", "_");
            try (PrintWriter w = new PrintWriter(new FileWriter(out + "/" + fn + ".c"))) { w.print(c); }
            println("OK " + want + " -> " + fn + ".c");
        }
    }
}
