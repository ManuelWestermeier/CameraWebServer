import React from "react";
import ReactDOM from "react-dom/client";
import { HashRouter, Routes, Route } from "react-router-dom";
import App from "./pages/App.jsx";
import Auth from "./pages/Auth.jsx";
import Viewer from "./pages/Viewer.jsx";

ReactDOM.createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <HashRouter>
      <Routes>
        <Route path="/" element={<App />} />
        <Route path="/auth" element={<Auth />} />
        <Route path="/viewer" element={<Viewer />} />
      </Routes>
    </HashRouter>
  </React.StrictMode>
);
