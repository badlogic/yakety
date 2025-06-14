# Update Documentation

You will generate comprehensive documentation to enable humans and LLMs to fully understand and work with this project.

## Your Task

Create documentation that allows humans and LLMs to:
- **Understand project purpose** - what the project does and why
- **Get architecture overview** - how the system is organized
- **Build on all platforms** - step-by-step build instructions
- **Add features/subsystems** - following established patterns and conventions
- **Debug applications** - troubleshoot issues at any level
- **Test and add tests** - run existing tests and create new ones
- **Deploy and distribute** - package and deploy the software

## Process

You will:
1. **Analyze the codebase in parallel** across 7 key areas of interest
2. **Create or update intermediate docs** in `docs/*.md` 
3. **Synthesize final documentation** into a minimal README.md
4. **Preserve all detailed files** for comprehensive reference

## Instructions

Analyze the codebase to create comprehensive documentation. For each area, check if an existing `docs/area-of-interest.md` file exists. If it exists, read it and update/improve the content based on the current codebase state. If it doesn't exist, create it from scratch. Always ensure the `docs/` directory exists before writing files.

Issue the following Task calls in parallel to speed up the process:

**Project Overview** (`docs/project-overview.md`):
- Check if `docs/project-overview.md` exists and read it if present
- Analyze the project to understand purpose, functionality, and key technologies
- Look at root files, source structure, dependencies, and external integrations
- Create new content or update/improve existing content with current findings
- Output: Project name, purpose, main functionality, key technologies, target platforms

**Architecture Analysis** (`docs/architecture.md`):
- Check if `docs/architecture.md` exists and read it if present
- Analyze source code architecture and organization
- Examine directory structure, key modules, component interactions, platform-specific code
- Create new content or update/improve existing content with current architecture state
- Output: Directory structure, key modules, component interactions, platform abstraction

**Build System** (`docs/build-system.md`):
- Check if `docs/build-system.md` exists and read it if present
- Analyze build configuration files (CMakeLists.txt, Makefile, package.json, Cargo.toml, etc.), build presets, dependencies, platform-specific requirements
- Evaluate required system tools and their installation requirements (compilers, build tools, package managers, etc.)
- Look for existing installation scripts or setup procedures
- Create new content or update/improve existing content with current build configuration
- Output: Build system overview, presets, dependencies, PLUS step-by-step instructions for: setting up build environment, building on each platform, troubleshooting common build issues

**Testing and Quality** (`docs/testing.md`):
- Check if `docs/testing.md` exists and read it if present
- Analyze test files, testing framework, code quality tools, CI/CD configuration
- Examine existing test patterns and conventions for guidance on adding new tests
- Create new content or update/improve existing content with current testing setup
- Output: Testing framework overview, test organization, PLUS step-by-step instructions for: running tests, adding new tests, debugging test failures, using code quality tools

**Development Workflow** (`docs/development.md`):
- Check if `docs/development.md` exists and read it if present
- Analyze development conventions, code style, Git workflow, development environment setup
- Examine debugging configurations, IDE setups, logging patterns, and troubleshooting approaches
- Look for development scripts, automation, and productivity tools
- Create new content or update/improve existing content with current conventions and practices
- Output: Code style overview, development environment, PLUS step-by-step instructions for: setting up development environment, debugging applications/features, using development tools, following Git workflow

**Deployment and Distribution** (`docs/deployment.md`):
- Check if `docs/deployment.md` exists and read it if present
- Analyze packaging, distribution, installation procedures, platform-specific deployment
- Examine server components, containerization, cloud deployment, infrastructure requirements
- Look for deployment scripts, configuration management, monitoring, and operational procedures
- Create new content or update/improve existing content with current deployment approach
- Output: Packaging overview, deployment architecture, PLUS step-by-step instructions for: packaging applications, deploying to different environments, configuring infrastructure, troubleshooting deployment issues

**Implementation Patterns** (`docs/patterns.md`):
- Check if `docs/patterns.md` exists and read it if present
- Analyze common implementation patterns in the codebase (e.g., async/concurrency patterns, abstraction layers, factory patterns, observer patterns, state management)
- Examine recurring code structures, design patterns, error handling approaches, resource management
- Look for project-specific patterns like UI abstractions, data flow patterns, plugin architectures, or domain-specific abstractions
- Identify patterns for adding new features/subsystems that follow established conventions
- Create new content or update/improve existing content with current implementation patterns
- Output: Design patterns overview, architectural patterns, PLUS step-by-step instructions for: adding new features following established patterns, creating new subsystems, extending existing components, following coding conventions

After all tasks complete, read all `docs/*.md` files and synthesize them into:
**README.md** - Create a minimal quick-start guide with just enough info to get up and running. Each section should have bare minimum content plus explicit links to detailed docs (e.g., "For detailed build instructions, see [docs/build-system.md](docs/build-system.md)"). Keep it super short and concise - just actionable overview with clear navigation to the comprehensive docs. Create from scratch or completely overwrite existing README.md.

**Final Cleanup Pass**: Perform a final cleanup of each generated `.md` file to:
- Merge any duplicated content
- Remove information that doesn't belong in their specific area of interest
- Make content minimal yet comprehensive
- Ensure clarity and actionability

## Requirements

- Create `docs/` directory if it doesn't exist
- Handle both creating new files and updating existing ones
- Make README.md concise yet comprehensive enough for immediate understanding
- Structure for quick reference with clear sections and code examples  
- Ensure LLM-friendly organization and human readability
- Focus on actionable information for both humans and AI assistants
- Create or completely overwrite README.md without confirmation