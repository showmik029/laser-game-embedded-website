import Image from "next/image";
import Link from "next/link";

export default async function Home() {
  return (
    <main
      style={{
        height: "calc(100dvh - 48px)",
        padding: "16px 24px",
        fontFamily: "Arial, sans-serif",
        display: "flex",
        flexDirection: "column",
        gap: "0px",
        overflowX: "hidden",
        overflowY: "auto",
        position: "relative",
      }}
    >
      <div
        style={{
          position: "absolute",
          top: "16px",
          right: "24px",
          zIndex: 2,
        }}
      >
        <Image src="/ll.png" alt="Laser Labs game icon" width={260} height={260} />
      </div>
      <section
        style={{
          display: "flex",
          flexDirection: "column",
          alignItems: "center",
          justifyContent: "center",
          textAlign: "center",
          minHeight: "40vh",
          flex: "0 0 auto",
        }}
      >
        <div
          style={{
            display: "flex",
            flexDirection: "column",
            alignItems: "center",
            gap: "8px",
            marginBottom: "12px",
          }}
        >
          <h1
            className="ll-title"
            style={{
              fontSize: "82px",
              margin: 0,
              lineHeight: 1,
              fontWeight: 400,
            }}
          >
            Laser Labs
          </h1>
        </div>
        <p
          className="ll-subtitle"
          style={{
            fontSize: "18px",
            width: "min(92vw, 820px)",
            margin: 0,
            lineHeight: 1.4,
            fontWeight: 600,
          }}
        >
          Welcome to our Embedded IoT project called LaserLabs!!!! We are
          group-7 and the members are Akib, Mikke, Jesse and Noel. This is an
          arcade themed laser tag game where you have different game modes with
          live scoreboard to compete with your friends! Enjoy the game!!
        </p>
        <div
          style={{
            marginTop: "16px",
            display: "flex",
            gap: "14px",
            flexWrap: "wrap",
            justifyContent: "center",
          }}
        >
          <Link
            href="/"
            style={{
              display: "inline-block",
              padding: "12px 22px",
              borderRadius: "999px",
              border: "1px solid var(--border)",
              background: "rgba(255,255,255,0.08)",
              color: "var(--foreground)",
              textDecoration: "none",
              fontWeight: 700,
              fontSize: "19px",
            }}
          >
            Home
          </Link>
          <Link
            href="/scoreboard"
            style={{
              display: "inline-block",
              padding: "12px 22px",
              borderRadius: "999px",
              border: "1px solid var(--border)",
              background: "rgba(255,255,255,0.08)",
              color: "var(--foreground)",
              textDecoration: "none",
              fontWeight: 700,
              fontSize: "19px",
            }}
          >
            Scoreboard
          </Link>
          <Link
            href="/readme"
            style={{
              display: "inline-block",
              padding: "12px 22px",
              borderRadius: "999px",
              border: "1px solid var(--border)",
              background: "rgba(255,255,255,0.08)",
              color: "var(--foreground)",
              textDecoration: "none",
              fontWeight: 700,
              fontSize: "19px",
            }}
          >
            Readme
          </Link>
        </div>
      </section>
      <section
        style={{
          marginTop: "14px",
          alignSelf: "center",
          width: "min(92vw, 500px)",
        }}
      >
        <Image
          src="/image.jpg"
          alt="Laser game action"
          width={1000}
          height={560}
          style={{
            width: "100%",
            maxWidth: "500px",
            maxHeight: "100%",
            height: "auto",
            objectFit: "contain",
            borderRadius: "16px",
            border: "none",
          }}
        />
      </section>
    </main>
  );
}
