import { initializeApp } from "https://www.gstatic.com/firebasejs/10.11.1/firebase-app.js";
import { getAuth, onAuthStateChanged, signOut } from "https://www.gstatic.com/firebasejs/10.11.1/firebase-auth.js";
import { getFirestore, doc, onSnapshot } from "https://www.gstatic.com/firebasejs/10.11.1/firebase-firestore.js";

const firebaseConfig = {
    apiKey: "AIzaSyCCUFL2o-xKRnvSSdJucc6l0qNtn7Y_dio",
    authDomain: "esp32-firebase-demo-8744d.firebaseapp.com",
    databaseURL: "https://esp32-firebase-demo-8744d-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "esp32-firebase-demo-8744d",
    storageBucket: "esp32-firebase-demo-8744d.appspot.com",
    messagingSenderId: "12459303336",
    appId: "1:12459303336:web:faeb7a71fd0d8f9ecabd9d",
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const auth = getAuth();
const db = getFirestore();

// Check authentication state
onAuthStateChanged(auth, (user) => {
    if (user) {
        const userUID = user.uid; // Get the unique user ID
        const docRef = doc(db, "users", userUID);

        // Real-time listener for changes in user data
        onSnapshot(docRef, (docSnap) => {
            if (docSnap.exists()) {
                const userData = docSnap.data();

                // Update UI with the latest user data
                document.getElementById("loggedUserFName").innerText = userData.firstName;
                document.getElementById("loggedUserLName").innerText = userData.lastName;
                document.getElementById("loggedUserEmail").innerText = userData.email;
                document.getElementById("loggedUserCredit").innerText = userData.credit || "N/A";

                // Generate QR Code only once (static for the user)
                if (!document.getElementById("qrCodeContainer").hasChildNodes()) {
                    const qrCodeData = `${userUID}`;
                    QRCode.toCanvas(document.getElementById("qrCodeContainer"), qrCodeData, (error) => {
                        if (error) {
                            console.error("QR Code generation failed:", error);
                        } else {
                            console.log("QR Code generated successfully.");
                        }
                    });
                }
            } else {
                console.error("No document found for the user.");
            }
        });
    } else {
        console.error("No user is signed in.");
        window.location.href = "index.html"; // Redirect to login if not authenticated
    }
});

// Logout functionality
document.getElementById("logout").addEventListener("click", () => {
    signOut(auth)
        .then(() => {
            window.location.href = "index.html";
        })
        .catch((error) => {
            console.error("Error signing out:", error);
        });
});
