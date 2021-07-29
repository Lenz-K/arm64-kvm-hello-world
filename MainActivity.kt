package edu.hm.karbaumer.lenz.kvm_test_app

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import edu.hm.karbaumer.lenz.kvm_test_app.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        val text = stringFromJNI()
        Log.i("MA", "Result: $text")

        val filename = "/dev/kvm"
        val file = File(filename)
        val canRead = file.canRead().toString()
        val canWrite = file.canWrite().toString()

        binding.sampleText.text = "C code returned: $text\n'/dev/kvm' Read/Write permissions: $canRead/$canWrite"
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'native-lib' library on application startup.
        init {
            System.loadLibrary("native-lib")
        }
    }
}